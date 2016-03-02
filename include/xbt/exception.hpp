/* Copyright (c) 2016. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <vector>
#include <type_traits>
#include <utility>

#include <xbt/base.h>

namespace simgrid {
namespace xbt {

/** Mixin class for exceptions with a backtrace */
class XBT_PUBLIC () BacktraceException {
  std::vector<void*> trace_;
public:
  BacktraceException() noexcept;
  virtual ~BacktraceException() noexcept;
  std::vector<void*> const& getBacktrace() { return trace_; }
};

/** Combine a given exception with BacktraceException */
template<class E>
class WithBacktraceException : public E, public BacktraceException {
public:
  WithBacktraceException(E e) : E(std::move(e)) {}
};

/** Throw a given exception with the current backtrace */
// This is modeled on std::throw_with_nested().
template<class E> inline
void throw_with_backtrace(E e) // [[noreturn]]
{
  throw WithBacktraceException<typename std::remove_reference<E>::type>(
    std::forward<E>(e));
}

/** Throw a given exception with the current backtrace
 *  and the nested current exception */
// This is modeled on std::throw_with_nested().
template<class E> inline
void throw_with_backtrace_and_nested(E&& e) // [[noreturn]]
{
  std::throw_with_nested(
    WithBacktraceException<typename std::remove_reference<E>::type>(
      std::forward<E>(e)));
}

}
}
