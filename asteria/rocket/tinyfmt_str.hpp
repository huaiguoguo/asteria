// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#ifndef ROCKET_TINYFMT_STR_HPP_
#define ROCKET_TINYFMT_STR_HPP_

#include "tinyfmt.hpp"
#include "tinybuf_str.hpp"

namespace rocket {

template<typename charT, typename traitsT = char_traits<charT>,
         typename allocT = allocator<charT>> class basic_tinyfmt_str;

template<typename charT, typename traitsT,
         typename allocT> class basic_tinyfmt_str : public basic_tinyfmt<charT, traitsT>
  {
  public:
    using char_type       = charT;
    using traits_type     = traitsT;
    using allocator_type  = allocT;

    using tinyfmt_type  = basic_tinyfmt<charT, traitsT>;
    using buffer_type   = basic_tinybuf_str<charT, traitsT>;
    using string_type   = typename buffer_type::string_type;

    using seek_dir   = typename buffer_type::seek_dir;
    using open_mode  = typename buffer_type::open_mode;
    using int_type   = typename buffer_type::int_type;
    using off_type   = typename buffer_type::off_type;
    using size_type  = typename buffer_type::size_type;

  private:
    mutable buffer_type m_buf;

  public:
    basic_tinyfmt_str() noexcept
      :
        m_buf(tinybuf_base::open_write)
      {
      }
    explicit basic_tinyfmt_str(open_mode mode) noexcept
      :
        m_buf(mode)
      {
      }
    template<typename xstrT> explicit basic_tinyfmt_str(xstrT&& xstr, open_mode mode = tinybuf_base::open_write)
      :
        m_buf(noadl::forward<xstrT>(xstr), mode)
      {
      }
    ~basic_tinyfmt_str() override;

    basic_tinyfmt_str(basic_tinyfmt_str&&)
      = default;
    basic_tinyfmt_str& operator=(basic_tinyfmt_str&&)
      = default;

  public:
    buffer_type& get_buffer() const override
      {
        return this->m_buf;
      }

    const string_type& get_string() const noexcept
      {
        return this->m_buf.get_string();
      }
    const char_type* get_c_string() const noexcept
      {
        return this->m_buf.get_c_string();
     }
    basic_tinyfmt_str& clear_string(open_mode mode = tinybuf_base::open_write)
      {
        return this->m_buf.clear_string(mode), *this;
      }
    template<typename xstrT>
        basic_tinyfmt_str& set_string(xstrT&& xstr, open_mode mode = tinybuf_base::open_write)
      {
        return this->m_buf.set_string(noadl::forward<xstrT>(xstr), mode), *this;
      }
    string_type extract_string(open_mode mode = tinybuf_base::open_write)
      {
        return this->m_buf.extract_string(mode);
      }

    void swap(basic_tinyfmt_str& other)
      {
        noadl::adl_swap(this->m_buf, other.m_buf);
      }
  };

template<typename charT, typename traitsT,
         typename allocT> basic_tinyfmt_str<charT, traitsT, allocT>::~basic_tinyfmt_str()
  = default;

template<typename charT, typename traitsT,
         typename allocT> void swap(basic_tinyfmt_str<charT, traitsT, allocT>& lhs,
                                    basic_tinyfmt_str<charT, traitsT, allocT>& rhs)
  {
    return lhs.swap(rhs);
  }

extern template class basic_tinyfmt_str<char>;
extern template class basic_tinyfmt_str<wchar_t>;

using tinyfmt_str   = basic_tinyfmt_str<char>;
using wtinyfmt_str  = basic_tinyfmt_str<wchar_t>;

}  // namespace rocket

#endif