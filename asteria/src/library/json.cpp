// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "json.hpp"
#include "../runtime/argument_reader.hpp"
#include "../runtime/global_context.hpp"
#include "../compiler/token_stream.hpp"
#include "../compiler/parser_error.hpp"
#include "../compiler/enums.hpp"
#include "../utilities.hpp"

namespace Asteria {
namespace {

class Indenter
  {
  public:
    virtual
    ~Indenter();

  public:
    virtual
    tinyfmt&
    break_line(tinyfmt& fmt)
    const
      = 0;

    virtual
    void
    increment_level()
      = 0;

    virtual
    void
    decrement_level()
      = 0;

    virtual
    bool
    has_indention()
    const noexcept
      = 0;
  };

Indenter::
~Indenter()
  {
  }

class Indenter_none
final
  : public Indenter
  {
  public:
    explicit
    Indenter_none()
      = default;

  public:
    tinyfmt&
    break_line(tinyfmt& fmt)
    const override
      { return fmt;  }

    void
    increment_level()
    override
      { }

    void
    decrement_level()
    override
      { }

    bool
    has_indention()
    const noexcept override
      { return false;  }
  };

class Indenter_string
final
  : public Indenter
  {
  private:
    cow_string m_add;
    cow_string m_cur;

  public:
    explicit
    Indenter_string(const cow_string& add)
      : m_add(add), m_cur(::rocket::sref("\n"))
      { }

  public:
    tinyfmt&
    break_line(tinyfmt& fmt)
    const override
      { return fmt << this->m_cur;  }

    void
    increment_level()
    override
      { this->m_cur.append(this->m_add);  }

    void
    decrement_level()
    override
      { this->m_cur.pop_back(this->m_add.size());  }

    bool
    has_indention()
    const noexcept override
      { return this->m_add.size();  }
  };

class Indenter_spaces
final
  : public Indenter
  {
  private:
    size_t m_add;
    size_t m_cur;

  public:
    explicit
    Indenter_spaces(int64_t add)
      : m_add(static_cast<size_t>(::rocket::clamp(add, 0, 10))), m_cur(0)
      { }

  public:
    tinyfmt&
    break_line(tinyfmt& fmt)
    const override
      { return fmt << pwrap(this->m_add, this->m_cur);  }

    void
    increment_level()
    override
      { this->m_cur += this->m_add;  }

    void
    decrement_level()
    override
      { this->m_cur -= this->m_add;  }

    bool
    has_indention()
    const noexcept override
      { return this->m_add;  }
  };

tinyfmt&
do_quote_string(tinyfmt& fmt, const cow_string& str)
  {
    // Although JavaScript uses UCS-2 rather than UTF-16, the JSON specification adopts UTF-16.
    fmt << '\"';
    size_t offset = 0;
    while(offset < str.size()) {
      // Convert UTF-8 to UTF-16.
      char32_t cp;
      if(!utf8_decode(cp, str, offset)) {
        // Invalid UTF-8 code units are replaced with the replacement character.
        cp = 0xFFFD;
      }
      // Escape double quotes, backslashes, and control characters.
      switch(cp) {
        case '\"':
          fmt << "\\\"";
          break;

        case '\\':
          fmt << "\\\\";
          break;

        case '\b':
          fmt << "\\b";
          break;

        case '\f':
          fmt << "\\f";
          break;

        case '\n':
          fmt << "\\n";
          break;

        case '\r':
          fmt << "\\r";
          break;

        case '\t':
          fmt << "\\t";
          break;

        default: {
          if((0x20 <= cp) && (cp <= 0x7E)) {
            // Write printable characters as is.
            fmt << static_cast<char>(cp);
            break;
          }
          // Encode the character in UTF-16.
          char16_t ustr[2];
          char16_t* epos = ustr;
          utf16_encode(epos, cp);
          // Write code units.
          ::rocket::ascii_numput nump;
          for(auto p = ustr;  p != epos;  ++p) {
            nump.put_XU(*p, 4);
            char seq[8] = { "\\u" };
            ::std::memcpy(seq + 2, nump.data() + 2, 4);
            fmt << ::rocket::sref(seq, 6);
          }
          break;
        }
      }
    }
    fmt << '\"';
    return fmt;
  }

tinyfmt&
do_quote_object_key(tinyfmt& fmt, bool json5, const cow_string& name)
  {
    if(json5 && name.size() && is_cctype(name[0], cctype_namei) &&
                ::std::all_of(name.begin() + 1, name.end(),
                              [](char c) { return is_cctype(c, cctype_namei | cctype_digit);  }))
      return fmt << name;
    else
      return do_quote_string(fmt, name);
  }

V_object::const_iterator
do_find_uncensored(const V_object& object, V_object::const_iterator from)
  {
    return ::std::find_if(from, object.end(),
        [](const auto& pair) {
          return ::rocket::is_any_of(pair.second.vtype(),
              { vtype_null, vtype_boolean, vtype_integer, vtype_real, vtype_string,
                vtype_array, vtype_object });
        });
  }

tinyfmt&
do_format_scalar(tinyfmt& fmt, const Value& value, bool json5)
  {
    switch(weaken_enum(value.vtype())) {
      case vtype_boolean:
        // Write `true` or `false`.
        return fmt << value.as_boolean();

      case vtype_integer:
        // Write the integer in decimal.
        return fmt << V_real(value.as_integer());

      case vtype_real: {
        // Is the value finite?
        switch(::std::fpclassify(value.as_real())) {
          case FP_INFINITE:
            if(!json5)
              break;
            // JSON5 allows `Infinity` in ECMAScript form.
            return fmt << "Infinity";

          case FP_NAN:
            if(!json5)
              break;
            // JSON5 allows `NaN` in ECMAScript form.
            return fmt << "NaN";

          default:
            // Write the real in decimal.
            return fmt << value.as_real();
        }
        break;
      }

      case vtype_string:
        // Write the quoted string.
        return do_quote_string(fmt, value.as_string());
    }
    // Anything else is censored to `null`.
    return fmt << "null";
  }

struct S_xformat_array
  {
    refp<const V_array> refa;
    V_array::const_iterator curp;
  };

struct S_xformat_object
  {
    refp<const V_object> refo;
    V_object::const_iterator curp;
  };

using Xformat = variant<S_xformat_array, S_xformat_object>;

V_string
do_format_nonrecursive(const Value& value, bool json5, Indenter& indent)
  {
    ::rocket::tinyfmt_str fmt;
    // Transform recursion to iteration using a handwritten stack.
    auto qvalue = ::std::addressof(value);
    cow_vector<Xformat> stack;
    for(;;) {
      // Find a leaf value. `qvalue` must always point to a valid value here.
      if(qvalue->is_array()){
        const auto& array = qvalue->as_array();
        // Open an array.
        fmt << '[';
        auto curp = array.begin();
        if(curp != array.end()) {
          // Indent the body.
          indent.increment_level();
          indent.break_line(fmt);
          // Decend into the array.
          S_xformat_array ctxa = { ::rocket::ref(array), curp };
          stack.emplace_back(::std::move(ctxa));
          qvalue = ::std::addressof(*curp);
          continue;
        }
        // Write an empty array.
        fmt << ']';
      }
      else if(qvalue->is_object()) {
        const auto& object = qvalue->as_object();
        // Open an object.
        fmt << '{';
        auto curp = do_find_uncensored(object, object.begin());
        if(curp != object.end()) {
          // Indent the body.
          indent.increment_level();
          indent.break_line(fmt);
          // Write the key followed by a colon.
          do_quote_object_key(fmt, json5, curp->first);
          fmt << ':';
          if(indent.has_indention())
            fmt << ' ';
          // Decend into the object.
          S_xformat_object ctxo = { ::rocket::ref(object), curp };
          stack.emplace_back(::std::move(ctxo));
          qvalue = ::std::addressof(curp->second);
          continue;
        }
        // Write an empty object.
        fmt << '}';
      }
      else {
        // Just write a scalar value which is never recursive.
        do_format_scalar(fmt, *qvalue, json5);
      }
      for(;;) {
        // Advance to the next element if any.
        if(stack.empty()) {
          // Finish the root value.
          return fmt.extract_string();
        }
        if(stack.back().index() == 0) {
          auto& ctxa = stack.mut_back().as<0>();
          // Advance to the next element.
          auto curp = ++(ctxa.curp);
          if(curp != ctxa.refa->end()) {
            // Add a comma between elements.
            fmt << ',';
            indent.break_line(fmt);
            // Format the next element.
            ctxa.curp = curp;
            qvalue = ::std::addressof(*curp);
            break;
          }
          // Add a trailing comma.
          if(json5 && indent.has_indention())
            fmt << ',';
          // Unindent the body.
          indent.decrement_level();
          indent.break_line(fmt);
          // Finish this array.
          fmt << ']';
        }
        else {
          auto& ctxo = stack.mut_back().as<1>();
          // Advance to the next key-value pair.
          auto curp = do_find_uncensored(ctxo.refo, ++(ctxo.curp));
          if(curp != ctxo.refo->end()) {
            // Add a comma between elements.
            fmt << ',';
            indent.break_line(fmt);
            // Write the key followed by a colon.
            do_quote_object_key(fmt, json5, curp->first);
            fmt << ':';
            if(indent.has_indention())
              fmt << ' ';
            // Format the next value.
            ctxo.curp = curp;
            qvalue = ::std::addressof(curp->second);
            break;
          }
          // Add a trailing comma.
          if(json5 && indent.has_indention())
            fmt << ',';
          // Unindent the body.
          indent.decrement_level();
          indent.break_line(fmt);
          // Finish this array.
          fmt << '}';
        }
        stack.pop_back();
      }
    }
  }

V_string
do_format_nonrecursive(const Value& value, bool json5, Indenter&& indent)
  {
    return do_format_nonrecursive(value, json5, indent);
  }

optV_string
do_accept_identifier_opt(Token_Stream& tstrm, initializer_list<const char*> accept)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok) {
      return nullopt;
    }
    if(qtok->is_identifier()) {
      auto name = qtok->as_identifier();
      if(::rocket::is_any_of(name, accept)) {
        tstrm.shift();
        // A match has been found.
        return ::std::move(name);
      }
    }
    return nullopt;
  }

opt<Punctuator>
do_accept_punctuator_opt(Token_Stream& tstrm, initializer_list<Punctuator> accept)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok) {
      return nullopt;
    }
    if(qtok->is_punctuator()) {
      auto punct = qtok->as_punctuator();
      if(::rocket::is_any_of(punct, accept)) {
        tstrm.shift();
        // A match has been found.
        return punct;
      }
    }
    return nullopt;
  }

optV_real
do_accept_number_opt(Token_Stream& tstrm)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok) {
      return nullopt;
    }
    if(qtok->is_integer_literal()) {
      auto val = qtok->as_integer_literal();
      tstrm.shift();
      // Convert integers to reals, as JSON does not have an integral type.
      return V_real(val);
    }
    if(qtok->is_real_literal()) {
      auto val = qtok->as_real_literal();
      tstrm.shift();
      // This real can be copied as is.
      return V_real(val);
    }
    if(qtok->is_punctuator()) {
      auto punct = qtok->as_punctuator();
      if(::rocket::is_any_of(punct, { punctuator_add, punctuator_sub })) {
        V_real sign = (punct == punctuator_sub) ? -1.0 : +1.0;
        // Only `Infinity` and `NaN` are allowed.
        // Please note that the tokenizer will have merged sign symbols into adjacent number literals.
        qtok = tstrm.peek_opt(1);
        if(!qtok) {
          return nullopt;
        }
        if(!qtok->is_identifier()) {
          return nullopt;
        }
        const auto& name = qtok->as_identifier();
        if(name == "Infinity") {
          tstrm.shift(2);
          // Accept a signed `Infinity`.
          return ::std::copysign(::std::numeric_limits<V_real>::infinity(), sign);
        }
        if(name == "NaN") {
          tstrm.shift(2);
          // Accept a signed `NaN`.
          return ::std::copysign(::std::numeric_limits<V_real>::quiet_NaN(), sign);
        }
      }
    }
    return nullopt;
  }

optV_string
do_accept_string_opt(Token_Stream& tstrm)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok) {
      return nullopt;
    }
    if(qtok->is_string_literal()) {
      auto val = qtok->as_string_literal();
      tstrm.shift();
      // This string literal can be copied as is in UTF-8.
      return V_string(::std::move(val));
    }
    return nullopt;
  }

opt<Value>
do_accept_scalar_opt(Token_Stream& tstrm)
  {
    auto qnum = do_accept_number_opt(tstrm);
    if(qnum)
      // Accept a `Number`.
      return *qnum;

    auto qstr = do_accept_string_opt(tstrm);
    if(qstr)
      // Accept a `String`.
      return ::std::move(*qstr);

    qstr = do_accept_identifier_opt(tstrm, { "true", "false", "Infinity", "NaN", "null" });
    if(qstr) {
      switch(qstr->front()) {
        case 't':
          // Accept a `Boolean` of value `true`.
          return true;

        case 'f':
          // Accept a `Boolean` of value `false`.
          return false;

        case 'I':
          // Accept a `Number` of value `Infinity`.
          return ::std::numeric_limits<V_real>::infinity();

        case 'N':
          // Accept a `Number` of value `NaN`.
          return ::std::numeric_limits<V_real>::quiet_NaN();

        default:
          // Accept an explicit `null`.
          return V_null();
      }
    }

    return nullopt;
  }

optV_string
do_accept_key_opt(Token_Stream& tstrm)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok) {
      return nullopt;
    }
    if(qtok->is_identifier()) {
      auto name = qtok->as_identifier();
      tstrm.shift();
      // Identifiers are allowed unquoted in JSON5.
      return V_string(::std::move(name));
    }
    if(qtok->is_string_literal()) {
      auto val = qtok->as_string_literal();
      tstrm.shift();
      // This string literal can be copied as is in UTF-8.
      return V_string(::std::move(val));
    }
    return nullopt;
  }

struct S_xparse_array
  {
    V_array array;
  };

struct S_xparse_object
  {
    V_object object;
    V_string key;
  };

using Xparse = variant<S_xparse_array, S_xparse_object>;

Value
do_json_parse_nonrecursive(Token_Stream& tstrm)
  {
    Value value;
    // Implement a recursive descent parser without recursion.
    cow_vector<Xparse> stack;
    for(;;) {
      // Accept a leaf value. No other things such as closed brackets are allowed.
      auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_op, punctuator_brace_op });
      if(kpunct == punctuator_bracket_op) {
        // An open bracket has been accepted.
        kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl });
        if(!kpunct) {
          // Descend into the new array.
          S_xparse_array ctxa = { nullopt };
          stack.emplace_back(::std::move(ctxa));
          continue;
        }
        // Accept an empty array.
        value = V_array();
      }
      else if(kpunct == punctuator_brace_op) {
        // An open brace has been accepted.
        kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl });
        if(!kpunct) {
          // A key followed by a colon is expected.
          auto qkey = do_accept_key_opt(tstrm);
          if(!qkey) {
            throw Parser_Error(parser_status_closed_brace_or_json5_key_expected, tstrm.next_sloc(),
                               tstrm.next_length());
          }
          kpunct = do_accept_punctuator_opt(tstrm, { punctuator_colon });
          if(!kpunct) {
            throw Parser_Error(parser_status_colon_expected, tstrm.next_sloc(), tstrm.next_length());
          }
          // Descend into a new object.
          S_xparse_object ctxo = { nullopt, ::std::move(*qkey) };
          stack.emplace_back(::std::move(ctxo));
          continue;
        }
        // Accept an empty object.
        value = V_object();
      }
      else {
        // Just accept a scalar value which is never recursive.
        auto qvalue = do_accept_scalar_opt(tstrm);
        if(!qvalue) {
          throw Parser_Error(parser_status_expression_expected, tstrm.next_sloc(), tstrm.next_length());
        }
        value = ::std::move(*qvalue);
      }
      // Insert the value into its parent array or object.
      for(;;) {
        if(stack.empty()) {
          // Accept the root value.
          return value;
        }
        if(stack.back().index() == 0) {
          auto& ctxa = stack.mut_back().as<0>();
          // Append the value to its parent array.
          ctxa.array.emplace_back(::std::move(value));
          // Look for the next element.
          kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl, punctuator_comma });
          if(!kpunct) {
            throw Parser_Error(parser_status_comma_expected, tstrm.next_sloc(), tstrm.next_length());
          }
          if(*kpunct == punctuator_comma) {
            kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl });
            if(!kpunct) {
              // The next element is expected to follow the comma.
              break;
            }
            // An extra comma is allowed in JSON5.
          }
          // Pop the array.
          value = ::std::move(ctxa.array);
        }
        else {
          auto& ctxo = stack.mut_back().as<1>();
          // Insert the value into its parent object.
          ctxo.object.insert_or_assign(::std::move(ctxo.key), ::std::move(value));
          // Look for the next element.
          kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl, punctuator_comma });
          if(!kpunct) {
            throw Parser_Error(parser_status_closed_brace_or_comma_expected, tstrm.next_sloc(),
                               tstrm.next_length());
          }
          if(*kpunct == punctuator_comma) {
            kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl });
            if(!kpunct) {
              // The next key is expected to follow the comma.
              auto qkey = do_accept_key_opt(tstrm);
              if(!qkey) {
                throw Parser_Error(parser_status_closed_brace_or_json5_key_expected, tstrm.next_sloc(),
                                   tstrm.next_length());
              }
              kpunct = do_accept_punctuator_opt(tstrm, { punctuator_colon });
              if(!kpunct) {
                throw Parser_Error(parser_status_colon_expected, tstrm.next_sloc(), tstrm.next_length());
              }
              ctxo.key = ::std::move(*qkey);
              // The next value is expected to follow the colon.
              break;
            }
            // An extra comma is allowed in JSON5.
          }
          // Pop the object.
          value = ::std::move(ctxo.object);
        }
        stack.pop_back();
      }
    }
  }

Value
do_json_parse(tinybuf& cbuf)
  try {
    // We reuse the lexer of Asteria here, allowing quite a few extensions e.g. binary numeric
    // literals and comments.
    Compiler_Options opts;
    opts.escapable_single_quotes = true;
    opts.keywords_as_identifiers = true;
    opts.integers_as_reals = true;

    Token_Stream tstrm(opts);
    tstrm.reload(cbuf, ::rocket::sref("<JSON text>"));
    if(tstrm.empty())
      ASTERIA_THROW("empty JSON string");

    // Parse a single value.
    auto value = do_json_parse_nonrecursive(tstrm);
    if(!tstrm.empty())
      ASTERIA_THROW("excess text at end of JSON string");
    return value;
  }
  catch(Parser_Error& except) {
    ASTERIA_THROW("invalid JSON string: $3 (line $1, offset $2)", except.line(), except.offset(),
                                                                  describe_parser_status(except.status()));
  }

}  // namespace

V_string
std_json_format(Value value, optV_string indent)
  {
    // No line break is inserted if `indent` is null or empty.
    return (!indent || indent->empty()) ? do_format_nonrecursive(value, false, Indenter_none())
                                        : do_format_nonrecursive(value, false, Indenter_string(*indent));
  }

V_string
std_json_format(Value value, V_integer indent)
  {
    // No line break is inserted if `indent` is non-positive.
    return (indent <= 0) ? do_format_nonrecursive(value, false, Indenter_none())
                         : do_format_nonrecursive(value, false, Indenter_spaces(indent));
  }

V_string
std_json_format5(Value value, optV_string indent)
  {
    // No line break is inserted if `indent` is null or empty.
    return (!indent || indent->empty()) ? do_format_nonrecursive(value, true, Indenter_none())
                                        : do_format_nonrecursive(value, true, Indenter_string(*indent));
  }

V_string
std_json_format5(Value value, V_integer indent)
  {
    // No line break is inserted if `indent` is non-positive.
    return (indent <= 0) ? do_format_nonrecursive(value, true, Indenter_none())
                         : do_format_nonrecursive(value, true, Indenter_spaces(indent));
  }

Value
std_json_parse(V_string text)
  {
    // Parse characters from the string.
    ::rocket::tinybuf_str cbuf;
    cbuf.set_string(text, tinybuf::open_read);
    return do_json_parse(cbuf);
  }

Value
std_json_parse_file(V_string path)
  {
    // Try opening the file.
    ::rocket::unique_posix_file fp(::fopen(path.safe_c_str(), "rb"), ::fclose);
    if(!fp)
      ASTERIA_THROW_SYSTEM_ERROR("fopen");

    // Parse characters from the file.
    ::setbuf(fp, nullptr);
    ::rocket::tinybuf_file cbuf(::std::move(fp));
    return do_json_parse(cbuf);
  }

void
create_bindings_json(V_object& result, API_Version /*version*/)
  {
    //===================================================================
    // `std.json.format()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("format"),
      V_function(
"""""""""""""""""""""""""""""""""""""""""""""""" R"'''''''''''''''(
`std.json.format(value, [indent])`

  * Converts a value to a string in the JSON format, according to
    IETF RFC 7159. This function generates text that conforms to
    JSON strictly; values whose types cannot be represented in JSON
    are discarded if they are found in an object and censored to
    `null` otherwise. If `indent` is set to a string, it is used as
    each level of indention following a line break, unless it is
    empty, in which case no line break is inserted. If `indent` is
    set to an integer, it is clamped between `0` and `10`
    inclusively and this function behaves as if a string consisting
    of this number of spaces was set. Its default value is an empty
    string.

  * Returns the formatted text as a string.
)'''''''''''''''" """""""""""""""""""""""""""""""""""""""""""""""",
*[](Reference& self, cow_vector<Reference>&& args, Global_Context& /*global*/) -> Reference&
  {
    Argument_Reader reader(::rocket::ref(args), ::rocket::sref("std.json.format"));
    Argument_Reader::State state;
    // Parse arguments.
    Value value;
    optV_string sindent;
    if(reader.I().o(value).S(state).o(sindent).F()) {
      Reference_root::S_temporary xref = { std_json_format(::std::move(value), ::std::move(sindent)) };
      return self = ::std::move(xref);
    }
    V_integer nindent;
    if(reader.L(state).v(nindent).F()) {
      Reference_root::S_temporary xref = { std_json_format(::std::move(value), ::std::move(nindent)) };
      return self = ::std::move(xref);
    }
    // Fail.
    reader.throw_no_matching_function_call();
  }
      ));

    //===================================================================
    // `std.json.format5()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("format5"),
      V_function(
"""""""""""""""""""""""""""""""""""""""""""""""" R"'''''''''''''''(
`std.json.format5(value, [indent])`

  * Converts a value to a string in the JSON5 format. In addition
    to IETF RFC 7159, a few extensions in ECMAScript 5.1 have been
    introduced to make the syntax more human-readable. Infinities
    and NaNs are preserved. Object keys that are valid identifiers
    are not quoted. Trailing commas of array and object members are
    appended if `indent` is neither null nor zero nor an empty
    string. This function is otherwise identical to `format()`.

  * Returns the formatted text as a string.
)'''''''''''''''" """""""""""""""""""""""""""""""""""""""""""""""",
*[](Reference& self, cow_vector<Reference>&& args, Global_Context& /*global*/) -> Reference&
  {
    Argument_Reader reader(::rocket::ref(args), ::rocket::sref("std.json.format5"));
    Argument_Reader::State state;
    // Parse arguments.
    Value value;
    optV_string sindent;
    if(reader.I().o(value).S(state).o(sindent).F()) {
      Reference_root::S_temporary xref = { std_json_format5(::std::move(value), ::std::move(sindent)) };
      return self = ::std::move(xref);
    }
    V_integer nindent;
    if(reader.L(state).v(nindent).F()) {
      Reference_root::S_temporary xref = { std_json_format5(::std::move(value), ::std::move(nindent)) };
      return self = ::std::move(xref);
    }
    // Fail.
    reader.throw_no_matching_function_call();
  }
      ));

    //===================================================================
    // `std.json.parse()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("parse"),
      V_function(
"""""""""""""""""""""""""""""""""""""""""""""""" R"'''''''''''''''(
`std.json.parse(text)`

  * Parses a string containing data encoded in the JSON format and
    converts it to a value. This function reuses the tokenizer of
    Asteria and allows quite a few extensions, some of which are
    also supported by JSON5:

    * Single-line and multiple-line comments are allowed.
    * Binary and hexadecimal numbers are allowed.
    * Numbers can have binary exponents.
    * Infinities and NaNs are allowed.
    * Numbers can start with plus signs.
    * Strings and object keys may be single-quoted.
    * Escape sequences (including UTF-32) are allowed in strings.
    * Element lists of arrays and objects may end in commas.
    * Object keys may be unquoted if they are valid identifiers.

    Be advised that numbers are always parsed as reals.

  * Returns the parsed value.

  * Throws an exception if the string is invalid.
)'''''''''''''''" """""""""""""""""""""""""""""""""""""""""""""""",
*[](Reference& self, cow_vector<Reference>&& args, Global_Context& /*global*/) -> Reference&
  {
    Argument_Reader reader(::rocket::ref(args), ::rocket::sref("std.json.parse"));
    // Parse arguments.
    V_string text;
    if(reader.I().v(text).F()) {
      Reference_root::S_temporary xref = { std_json_parse(::std::move(text)) };
      return self = ::std::move(xref);
    }
    // Fail.
    reader.throw_no_matching_function_call();
  }
      ));

    //===================================================================
    // `std.json.parse_file()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("parse_file"),
      V_function(
"""""""""""""""""""""""""""""""""""""""""""""""" R"'''''''''''''''(
`std.json.parse_file(path)`

  * Parses the contents of the file denoted by `path` as a JSON
    string. This function behaves identical to `parse()` otherwise.

  * Returns the parsed value.

  * Throws an exception if a read error occurs, or if the string is
    invalid.
)'''''''''''''''" """""""""""""""""""""""""""""""""""""""""""""""",
*[](Reference& self, cow_vector<Reference>&& args, Global_Context& /*global*/) -> Reference&
  {
    Argument_Reader reader(::rocket::ref(args), ::rocket::sref("std.json.parse_file"));
    // Parse arguments.
    V_string path;
    if(reader.I().v(path).F()) {
      Reference_root::S_temporary xref = { std_json_parse_file(::std::move(path)) };
      return self = ::std::move(xref);
    }
    // Fail.
    reader.throw_no_matching_function_call();
  }
      ));
  }

}  // namespace Asteria
