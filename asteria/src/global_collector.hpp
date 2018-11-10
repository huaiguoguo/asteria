// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_GLOBAL_COLLECTOR_HPP_
#define ASTERIA_GLOBAL_COLLECTOR_HPP_

#include "fwd.hpp"
#include "collector.hpp"

namespace Asteria {

class Global_collector : public rocket::refcounted_base<Global_collector>
  {
  private:
    Collector m_gen_two;
    Collector m_gen_one;
    Collector m_gen_zero;

  public:
    Global_collector() noexcept
      : m_gen_two(nullptr, 20),
        m_gen_one(&(this->m_gen_two), 100),
        m_gen_zero(&(this->m_gen_one), 500)
      {
      }
    ROCKET_NONCOPYABLE_DESTRUCTOR(Global_collector);

  public:
    rocket::refcounted_ptr<Variable> create_tracked_variable(const Reference *src_opt, bool immutable);
    bool untrack_variable(const rocket::refcounted_ptr<Variable> &var) noexcept;
    void perform_garbage_collection(unsigned gen_limit);
  };

}

#endif
