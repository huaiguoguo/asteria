// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "token_stream.hpp"
#include "enums.hpp"
#include "token.hpp"
#include "parser_error.hpp"
#include "../utilities.hpp"

namespace Asteria {
namespace {

class Line_Reader
  {
  private:
    refp<tinybuf> m_cbuf;
    cow_string m_file;

    size_t m_line = 0;
    size_t m_off = 0;
    cow_string m_str;

  public:
    Line_Reader(refp<tinybuf> xcbuf, const cow_string& xfile)
      : m_cbuf(xcbuf), m_file(xfile)
      { }

    ASTERIA_DECLARE_NONCOPYABLE(Line_Reader);

  public:
    tinybuf&
    cbuf()
    const noexcept
      { return this->m_cbuf;  }

    const cow_string&
    file()
    const noexcept
      { return this->m_file;  }

    int
    line()
    const noexcept
      { return static_cast<int>(this->m_line);  }

    int
    offset()
    const noexcept
      { return static_cast<int>(this->m_off);  }

    Source_Location
    tell()
    const noexcept
      { return Source_Location(this->file(), this->line(), this->offset());  }

    bool
    advance()
      {
        // Clear the current line buffer.
        this->m_str.clear();
        this->m_off = 0;

        // Buffer a line.
        for(;;) {
          int ch = this->m_cbuf->getc();
          if(ch == EOF) {
            // When the EOF is encountered, ...
            if(this->m_str.empty()) {
              // ... if the last line is empty, fail; ...
              return false;
            }
            // ... otherwise, accept the last line which does not end in an LF anyway.
            break;
          }
          if(ch == '\n') {
            // Accept a line without the LF.
            break;
          }
          if(this->m_str.size() >= INT_MAX) {
            ASTERIA_THROW("too many characters in a single line");
          }
          // Append the character to the line buffer.
          this->m_str.push_back(static_cast<char>(ch));
        }

        // Increment the line number if a line has been read successfully.
        if(this->m_line >= INT_MAX) {
          ASTERIA_THROW("too many lines in source code");
        }
        this->m_line++;

        // Accept the line.
        return true;
      }

    size_t
    navail()
    const noexcept
      {
        return this->m_str.size() - this->m_off;
      }

    const char*
    data(size_t add = 0)
    const
      {
        if(add > this->m_str.size() - this->m_off)
          ASTERIA_THROW("attempt to seek past end of line (`$1` + `$2` > `$3`)",
                        this->m_off, add, this->m_str.size());
        return this->m_str.data() + (this->m_off + add);
      }

    char
    peek(size_t add = 0)
    const noexcept
      {
        if(add > this->m_str.size() - this->m_off)
          return 0;
        return this->m_str[this->m_off + add];
      }

    void
    consume(size_t add)
      {
        if(add > this->m_str.size() - this->m_off)
          ASTERIA_THROW("attempt to seek past end of line (`$1` + `$2` > `$3`)",
                        this->m_off, add, this->m_str.size());
        this->m_off += add;
      }

    void
    rewind(size_t off = 0)
      {
        if(off > this->m_str.size())
          ASTERIA_THROW("invalid offset within current line (`$1` > `$2`)",
                        off, this->m_str.size());
        this->m_off = off;
      }
  };

class Tack
  {
  private:
    Source_Location m_sloc;
    size_t m_length = 0;

  public:
    explicit operator
    bool()
    const noexcept
      { return this->m_sloc.line() != -1;  }

    Source_Location
    tell()
    const noexcept
      { return this->m_sloc;  }

    size_t
    length()
    const noexcept
      { return this->m_length;  }

    Tack&
    set(const Line_Reader& reader, size_t xlength)
    noexcept
      {
        this->m_sloc = reader.tell();
        this->m_length = xlength;
        return *this;
      }

    Tack&
    clear()
    noexcept
      {
        this->m_sloc = nullopt;
        this->m_length = 0;
        return *this;
      }
  };

template<typename XTokenT>
bool
do_push_token(cow_vector<Token>& tokens, Line_Reader& reader, size_t tlen, XTokenT&& xtoken)
  {
    tokens.emplace_back(reader.tell(), tlen, ::std::forward<XTokenT>(xtoken));
    reader.consume(tlen);
    return true;
  }

bool
do_may_infix_operators_follow(cow_vector<Token>& tokens)
  {
    if(tokens.empty()) {
      // No previous token exists.
      return false;
    }
    const auto& p = tokens.back();

    // Infix operators may follow if the keyword denotes a value or reference.
    if(p.is_keyword())
      return ::rocket::is_any_of(p.as_keyword(),
                 { keyword_null, keyword_true, keyword_false, keyword_nan,
                   keyword_infinity, keyword_this });

    // Infix operators may follow if the punctuator can terminate an expression.
    if(p.is_punctuator())
      return ::rocket::is_any_of(p.as_punctuator(),
                 { punctuator_inc, punctuator_dec, punctuator_head, punctuator_tail,
                   punctuator_parenth_cl, punctuator_bracket_cl, punctuator_brace_cl });

    // Infix operators can follow.
    return true;
  }

cow_string&
do_collect_digits(cow_string& tstr, Line_Reader& reader, size_t& tlen, uint8_t mask)
  {
    for(;;) {
      char c = reader.peek(tlen);
      if(c == '`') {
        // Skip a digit separator.
        tlen++;
        continue;
      }
      if(!is_cctype(c, mask))
        break;

      // Collect a digit.
      tstr.push_back(c);
      tlen++;
    }
    return tstr;
  }

bool
do_accept_numeric_literal(cow_vector<Token>& tokens, Line_Reader& reader, bool integers_as_reals)
  {
    // numeric-literal ::=
    //   number-sign-opt ( binary-literal | decimal-literal | hexadecimal-literal ) exponent-suffix-opt
    // number-sign-opt ::=
    //   PCRE([+-]?)
    // binary-literal ::=
    //   PCRE(0[bB]([01]`?)+(\.([01]`?)+))
    // decimal-literal ::=
    //   PCRE(([0-9]`?)+(\.([0-9]`?)+))
    // hexadecimal-literal ::=
    //   PCRE(0[xX]([0-9A-Fa-f]`?)+(\.([0-9A-Fa-f]`?)+))
    // exponent-suffix-opt ::=
    //   decimal-exponent-suffix | binary-exponent-suffix | ""
    // decimal-exponent-suffix ::=
    //   PCRE([eE][-+]?([0-9]`?)+)
    // binary-exponent-suffix ::=
    //   PCRE([pP][-+]?([0-9]`?)+)
    cow_string tstr;
    size_t tlen = 0;
    // Look for an explicit sign symbol.
    switch(reader.peek(tlen)) {
      case '+':
        tstr = ::rocket::sref("+");
        tlen++;
        break;

      case '-':
        tstr = ::rocket::sref("-");
        tlen++;
        break;
    }
    // If a sign symbol exists in a context where an infix operator is allowed, it is treated as the latter.
    if((tlen != 0) && do_may_infix_operators_follow(tokens))
      return false;

    if(!is_cctype(reader.peek(tlen), cctype_digit))
      return false;

    // These are characterstics of the literal.
    uint8_t mmask = cctype_digit;
    uint8_t expch = 'e';
    bool has_point = false;
    // Get the mask of mantissa digits and tell which character initiates the exponent.
    if(reader.peek(tlen) == '0') {
      tstr += reader.peek(tlen);
      tlen++;
      // Check the radix identifier.
      if(::rocket::is_any_of(static_cast<uint8_t>(reader.peek(tlen) | 0x20), { 'b', 'x' })) {
        tstr += reader.peek(tlen);
        tlen++;
        // Accept the radix identifier.
        mmask = cctype_xdigit;
        expch = 'p';
      }
    }
    // Accept the longest string composing the integral part.
    do_collect_digits(tstr, reader, tlen, mmask);
    // Check for a radix point. If one exists, the fractional part shall follow.
    if(reader.peek(tlen) == '.') {
      tstr += reader.peek(tlen);
      tlen++;
      // Accept the fractional part.
      has_point = true;
      do_collect_digits(tstr, reader, tlen, mmask);
    }
    // Check for the exponent.
    if(static_cast<uint8_t>(reader.peek(tlen) | 0x20) == expch) {
      tstr += reader.peek(tlen);
      tlen++;
      // Check for an optional sign symbol.
      if(::rocket::is_any_of(reader.peek(tlen), { '+', '-' })) {
        tstr += reader.peek(tlen);
        tlen++;
      }
      // Accept the exponent.
      do_collect_digits(tstr, reader, tlen, cctype_digit);
    }
    // Accept numeric suffixes.
    // Note, at the moment we make no use of such suffixes, so any suffix will definitely cause parser errors.
    do_collect_digits(tstr, reader, tlen, cctype_alpha | cctype_digit);
    // Convert the token to a literal.
    // We always parse the literal as a floating-point number.
    ::rocket::ascii_numget numg;
    const char* bp = tstr.c_str();
    const char* ep = bp + tstr.size();
    if(!numg.parse_F(bp, ep))
      throw Parser_Error(parser_status_numeric_literal_invalid, reader.tell(), tlen);

    if(bp != ep)
      throw Parser_Error(parser_status_numeric_literal_suffix_invalid, reader.tell(), tlen);

    // It is cast to an integer only when `integers_as_reals` is `false` and it does not contain a radix point.
    if(!integers_as_reals && !has_point) {
      // Try casting the value to an `integer`.
      Token::S_integer_literal xtoken;
      numg.cast_I(xtoken.val, INT64_MIN, INT64_MAX);
      // Check for errors. Note that integer casts never underflows.
      if(numg.overflowed())
        throw Parser_Error(parser_status_integer_literal_overflow, reader.tell(), tlen);

      if(numg.inexact())
        throw Parser_Error(parser_status_integer_literal_inexact, reader.tell(), tlen);

      if(!numg)
        throw Parser_Error(parser_status_numeric_literal_invalid, reader.tell(), tlen);

      return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
    }
    else {
      // Try casting the value to a `real`.
      Token::S_real_literal xtoken;
      numg.cast_F(xtoken.val, -DBL_MAX, DBL_MAX);
      // Check for errors. Note that integer casts are never inexact.
      if(numg.overflowed())
        throw Parser_Error(parser_status_real_literal_overflow, reader.tell(), tlen);

      if(numg.underflowed())
        throw Parser_Error(parser_status_real_literal_underflow, reader.tell(), tlen);

      if(!numg)
        throw Parser_Error(parser_status_numeric_literal_invalid, reader.tell(), tlen);

      return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
    }
  }

struct Prefix_Comparator
  {
    template<typename ElementT>
    bool
    operator()(const ElementT& lhs, const ElementT& rhs)
    const noexcept
      { return ::rocket::char_traits<char>::compare(lhs.first, rhs.first, sizeof(lhs.first)) < 0;  }

    template<typename ElementT>
    bool
    operator()(char lhs, const ElementT& rhs)
    const noexcept
      { return ::rocket::char_traits<char>::lt(lhs, rhs.first[0]);  }

    template<typename ElementT>
    bool
    operator()(const ElementT& lhs, char rhs)
    const noexcept
      { return ::rocket::char_traits<char>::lt(lhs.first[0], rhs);  }
  };

struct Punctuator_Element
  {
    char first[6];
    Punctuator second;
  }
constexpr s_punctuators[] =
  {
    { "!",     punctuator_notl        },
    { "!=",    punctuator_cmp_ne      },
    { "%",     punctuator_mod         },
    { "%=",    punctuator_mod_eq      },
    { "&",     punctuator_andb        },
    { "&&",    punctuator_andl        },
    { "&&=",   punctuator_andl_eq     },
    { "&=",    punctuator_andb_eq     },
    { "(",     punctuator_parenth_op  },
    { ")",     punctuator_parenth_cl  },
    { "*",     punctuator_mul         },
    { "*=",    punctuator_mul_eq      },
    { "+",     punctuator_add         },
    { "++",    punctuator_inc         },
    { "+=",    punctuator_add_eq      },
    { ",",     punctuator_comma       },
    { "-",     punctuator_sub         },
    { "--",    punctuator_dec         },
    { "-=",    punctuator_sub_eq      },
    { ".",     punctuator_dot         },
    { "...",   punctuator_ellipsis    },
    { "/",     punctuator_div         },
    { "/=",    punctuator_div_eq      },
    { ":",     punctuator_colon       },
    { ";",     punctuator_semicol     },
    { "<",     punctuator_cmp_lt      },
    { "<<",    punctuator_sla         },
    { "<<<",   punctuator_sll         },
    { "<<<=",  punctuator_sll_eq      },
    { "<<=",   punctuator_sla_eq      },
    { "<=",    punctuator_cmp_lte     },
    { "<=>",   punctuator_spaceship   },
    { "=",     punctuator_assign      },
    { "==",    punctuator_cmp_eq      },
    { ">",     punctuator_cmp_gt      },
    { ">=",    punctuator_cmp_gte     },
    { ">>",    punctuator_sra         },
    { ">>=",   punctuator_sra_eq      },
    { ">>>",   punctuator_srl         },
    { ">>>=",  punctuator_srl_eq      },
    { "?",     punctuator_quest       },
    { "?=",    punctuator_quest_eq    },
    { "?\?",   punctuator_coales      },
    { "?\?=",  punctuator_coales_eq   },
    { "[",     punctuator_bracket_op  },
    { "[$]",   punctuator_tail        },
    { "[^]",   punctuator_head        },
    { "]",     punctuator_bracket_cl  },
    { "^",     punctuator_xorb        },
    { "^=",    punctuator_xorb_eq     },
    { "{",     punctuator_brace_op    },
    { "|",     punctuator_orb         },
    { "|=",    punctuator_orb_eq      },
    { "||",    punctuator_orl         },
    { "||=",   punctuator_orl_eq      },
    { "}",     punctuator_brace_cl    },
    { "~",     punctuator_notb        },
  };

bool
do_accept_punctuator(cow_vector<Token>& tokens, Line_Reader& reader)
  {
#ifdef ROCKET_DEBUG
    ROCKET_ASSERT(::std::is_sorted(begin(s_punctuators), end(s_punctuators), Prefix_Comparator()));
#endif
    // For two elements X and Y, if X is in front of Y, then X is potential a prefix of Y.
    // Traverse the range backwards to prevent premature matches, as a token is defined to be the longest valid
    // character sequence.
    auto range = ::std::equal_range(begin(s_punctuators), end(s_punctuators), reader.peek(), Prefix_Comparator());
    for(;;) {
      if(range.first == range.second) {
        // No matching punctuator has been found so far.
        return false;
      }
      const auto& cur = range.second[-1];
      // Has a match been found?
      auto tlen = ::std::strlen(cur.first);
      if((tlen <= reader.navail()) && (::std::memcmp(reader.data(), cur.first, tlen) == 0)) {
        // A punctuator has been found.
        Token::S_punctuator xtoken = { cur.second };
        return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
      }
      range.second--;
    }
  }

bool
do_accept_string_literal(cow_vector<Token>& tokens, Line_Reader& reader, char head, bool escapable)
  {
    // string-literal ::=
    //   escape-string-literal | noescape-string-literal
    // escape-string-literal ::=
    //   PCRE("([^\\]|(\\([abfnrtveZ0'"?\\/]|(x[0-9A-Fa-f]{2})|(u[0-9A-Fa-f]{4})|(U[0-9A-Fa-f]{6}))))*?")
    // noescape-string-literal ::=
    //   PCRE('[^']*?')
    if(reader.peek() != head)
      return false;

    // Get a string literal.
    size_t tlen = 1;
    cow_string val;
    for(;;) {
      // Read a character.
      char next = reader.peek(tlen);
      if(next == 0)
        throw Parser_Error(parser_status_string_literal_unclosed, reader.tell(), tlen);

      tlen++;

      // Check it.
      if(next == head) {
        // The end of this string is encountered. Finish.
        break;
      }
      if(!escapable || (next != '\\')) {
        // This character does not start an escape sequence. Copy it as is.
        val.push_back(next);
        continue;
      }

      // Translate this escape sequence.
      // Read the next charactter.
      next = reader.peek(tlen);
      if(next == 0)
        throw Parser_Error(parser_status_escape_sequence_incomplete, reader.tell(), tlen);

      tlen++;

      // Translate it.
      int xcnt = 0;
      switch(next) {
        case '\'':
        case '\"':
        case '\\':
        case '?':
        case '/':
          val.push_back(next);
          break;

        case 'a':
          val.push_back('\a');
          break;

        case 'b':
          val.push_back('\b');
          break;

        case 'f':
          val.push_back('\f');
          break;

        case 'n':
          val.push_back('\n');
          break;

        case 'r':
          val.push_back('\r');
          break;

        case 't':
          val.push_back('\t');
          break;

        case 'v':
          val.push_back('\v');
          break;

        case '0':
          val.push_back('\0');
          break;

        case 'Z':
          val.push_back('\x1A');
          break;

        case 'e':
          val.push_back('\x1B');
          break;

        case 'U':
          xcnt += 2;
          // Fallthrough
        case 'u':
          xcnt += 2;
          // Fallthrough
        case 'x': {
          // How many hex digits are there?
          xcnt += 2;
          // Read hex digits.
          char32_t cp = 0;
          for(int i = 0;  i < xcnt;  ++i) {
            // Read a hex digit.
            char c = reader.peek(tlen);
            if(c == 0)
              throw Parser_Error(parser_status_escape_sequence_incomplete, reader.tell(), tlen);

            if(!is_cctype(c, cctype_xdigit))
              throw Parser_Error(parser_status_escape_sequence_invalid_hex, reader.tell(), tlen);

            tlen++;
            // Accumulate this digit.
            cp *= 16;
            cp += static_cast<uint32_t>((c <= '9') ? (c - '0') : ((c | 0x20) - 'a' + 10));
          }
          if(next == 'x') {
            // Write the character verbatim.
            val.push_back(static_cast<char>(cp));
            break;
          }
          // Write a Unicode code point.
          if(!utf8_encode(val, cp))
            throw Parser_Error(parser_status_escape_utf_code_point_invalid, reader.tell(), tlen);

          break;
        }

        default:
          throw Parser_Error(parser_status_escape_sequence_unknown, reader.tell(), tlen);
      }
    }

    Token::S_string_literal xtoken = { ::std::move(val) };
    return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
  }

struct Keyword_Element
  {
    char first[10];
    Keyword second;
  }
constexpr s_keywords[] =
  {
    { "__abs",     keyword_abs       },
    { "__ceil",    keyword_ceil      },
    { "__ceili",   keyword_ceili     },
    { "__floor",   keyword_floor     },
    { "__floori",  keyword_floori    },
    { "__fma",     keyword_fma       },
    { "__global",  keyword_global    },
    { "__isinf",   keyword_isinf     },
    { "__isnan",   keyword_isnan     },
    { "__round",   keyword_round     },
    { "__roundi",  keyword_roundi    },
    { "__sign",    keyword_sign      },
    { "__sqrt",    keyword_sqrt      },
    { "__trunc",   keyword_trunc     },
    { "__trunci",  keyword_trunci    },
    { "__vcall",   keyword_vcall     },
    { "and",       keyword_and       },
    { "assert",    keyword_assert    },
    { "break",     keyword_break     },
    { "case",      keyword_case      },
    { "catch",     keyword_catch     },
    { "const",     keyword_const     },
    { "continue",  keyword_continue  },
    { "countof",   keyword_countof   },
    { "default",   keyword_default   },
    { "defer",     keyword_defer     },
    { "do",        keyword_do        },
    { "each",      keyword_each      },
    { "else",      keyword_else      },
    { "false",     keyword_false     },
    { "for",       keyword_for       },
    { "func",      keyword_func      },
    { "if",        keyword_if        },
    { "import",    keyword_import    },
    { "infinity",  keyword_infinity  },
    { "nan",       keyword_nan       },
    { "not",       keyword_not       },
    { "null",      keyword_null      },
    { "or",        keyword_or        },
    { "return",    keyword_return    },
    { "switch",    keyword_switch    },
    { "this",      keyword_this      },
    { "throw",     keyword_throw     },
    { "true",      keyword_true      },
    { "try",       keyword_try       },
    { "typeof",    keyword_typeof    },
    { "unset",     keyword_unset     },
    { "var",       keyword_var       },
    { "while",     keyword_while     },
  };

bool
do_accept_identifier_or_keyword(cow_vector<Token>& tokens, Line_Reader& reader, bool keywords_as_identifiers)
  {
    // identifier ::=
    //   PCRE([A-Za-z_][A-Za-z_0-9]*)
    if(!is_cctype(reader.peek(), cctype_namei))
      return false;

    // Get the length of this identifier.
    size_t tlen = 1;
    for(;;) {
      char next = reader.peek(tlen);
      if(next == 0)
        break;
      if(!is_cctype(next, cctype_namei | cctype_digit))
        break;
      tlen++;
    }
    if(keywords_as_identifiers) {
      // Do not check for identifiers.
      Token::S_identifier xtoken = { cow_string(reader.data(), tlen) };
      return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
    }
#ifdef ROCKET_DEBUG
    ROCKET_ASSERT(::std::is_sorted(begin(s_keywords), end(s_keywords), Prefix_Comparator()));
#endif
    auto range = ::std::equal_range(begin(s_keywords), end(s_keywords), reader.peek(), Prefix_Comparator());
    for(;;) {
      if(range.first == range.second) {
        // No matching keyword has been found so far.
        Token::S_identifier xtoken = { cow_string(reader.data(), tlen) };
        return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
      }
      const auto& cur = range.first[0];
      if((::std::strlen(cur.first) == tlen) && (::std::memcmp(reader.data(), cur.first, tlen) == 0)) {
        // A keyword has been found.
        Token::S_keyword xtoken = { cur.second };
        return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
      }
      range.first++;
    }
  }

}  // namespace

Token_Stream::
~Token_Stream()
  {
  }

Token_Stream&
Token_Stream::
reload(tinybuf& cbuf, const cow_string& file)
  {
    // Tokens are parsed and stored here in normal order.
    // We will have to reverse this sequence before storing it into `*this` if it is accepted.
    cow_vector<Token> tokens;
    // Destroy the contents of `*this` and reuse their storage, if any.
    tokens.swap(this->m_rtoks);
    tokens.clear();

    // Save the position of an unterminated block comment.
    Tack bcomm;
    // Read source code line by line.
    Line_Reader reader(::rocket::ref(cbuf), file);

    while(reader.advance()) {
      // Discard the first line if it looks like a shebang.
      if((reader.line() == 1) && (::std::strncmp(reader.data(), "#!", 2) == 0))
        continue;

      // Ensure this line is a valid UTF-8 string.
      while(reader.navail() != 0) {
        // Decode a code point.
        char32_t cp;
        auto tptr = reader.data();
        if(!utf8_decode(cp, tptr, reader.navail()))
          throw Parser_Error(parser_status_utf8_sequence_invalid, reader.tell(), reader.navail());

        auto u8len = static_cast<size_t>(tptr - reader.data());
        // Disallow plain null characters in source data.
        if(cp == 0)
          throw Parser_Error(parser_status_null_character_disallowed, reader.tell(), u8len);

        // Accept this code point.
        reader.consume(u8len);
      }

      // Re-scan this line from the beginning.
      reader.rewind();

      // Break this line down into tokens.
      while(reader.navail() != 0) {
        // Are we inside a block comment?
        if(bcomm) {
          // Search for the terminator of this block comment.
          auto tptr = ::std::strstr(reader.data(), "*/");
          if(!tptr) {
            // The block comment will not end in this line. Stop.
            break;
          }
          auto tlen = static_cast<size_t>(tptr + 2 - reader.data());
          // Finish this comment and resume from the end of it.
          bcomm.clear();
          reader.consume(tlen);
          continue;
        }
        // Read a character.
        if(is_cctype(reader.peek(), cctype_space)) {
          // Skip a space.
          reader.consume(1);
          continue;
        }
        if(reader.peek() == '/') {
          if(reader.peek(1) == '/') {
            // Start a line comment. Discard all remaining characters in this line.
            break;
          }
          if(reader.peek(1) == '*') {
            // Start a block comment.
            bcomm.set(reader, 2);
            reader.consume(2);
            continue;
          }
        }
        bool token_got = do_accept_numeric_literal(tokens, reader, this->m_opts.integers_as_reals) ||
                         do_accept_punctuator(tokens, reader) ||
                         do_accept_string_literal(tokens, reader, '\"', true) ||
                         do_accept_string_literal(tokens, reader, '\'', this->m_opts.escapable_single_quotes) ||
                         do_accept_identifier_or_keyword(tokens, reader, this->m_opts.keywords_as_identifiers);
        if(!token_got)
          throw Parser_Error(parser_status_token_character_unrecognized, reader.tell(), 1);
      }
    }
    if(bcomm)
      // A block comment may straddle multiple lines. We just mark the first line here.
      throw Parser_Error(parser_status_block_comment_unclosed, bcomm.tell(), bcomm.length());

    // Reverse the token sequence now.
    ::std::reverse(tokens.mut_begin(), tokens.mut_end());
    // Succeed.
    this->m_rtoks = ::std::move(tokens);
    return *this;
  }

}  // namespace Asteria
