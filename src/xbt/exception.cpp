/* Copyright (c) 2016. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <xbt/ex.h>
#include <xbt/exception.hpp>

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(xbt_exception, xbt,
  "Exceptions with backtraces");

namespace simgrid {
namespace xbt {

BacktraceException::BacktraceException() noexcept
{
  const std::size_t size = 128;
  void* buffer[size];
  int count = xbt_backtrace_no_malloc(buffer, size);
  try {
    trace_.assign(buffer, buffer + count);
  }

  // Don't throw exceptions when generating an exception:
  catch(std::exception& e) {
    XBT_DEBUG("Exception while trying to get a baktrace, %s", e.what());
  }
  catch(...) {
    XBT_DEBUG("Unknown exception while trying to get a baktrace");
  }
}

BacktraceException::~BacktraceException() noexcept
{

}

}
}

#ifdef SIMGRID_TEST

#include <cstring>

#include <exception>
#include <stdexcept>

#include <xbt/exception.hpp>

extern "C" {

XBT_TEST_SUITE("BacktraceException", "Backtraces with exceptions");

}

template<class A, class B> static
bool instance_of(B& b)
{
  return dynamic_cast<A*>(&b) != nullptr;
}

extern "C" {

static const char* message = "Hello world!";
static const char* message2 = "Hello again!";

XBT_TEST_UNIT("backtrace", test_exception_backtrace, "Throw an exception with a backtrace")
{
  xbt_test_add("Throw exception with backtrace");
  try {
    simgrid::xbt::throw_with_backtrace(std::runtime_error(message));
  }
  catch (std::exception& e) {
    xbt_test_assert(std::strcmp(message, e.what()) == 0,
      "Same message");
    xbt_test_assert(instance_of<std::runtime_error>(e),
      "Not a runtime_error");
    xbt_test_assert(instance_of<simgrid::xbt::BacktraceException>(e),
      "Not a BacktraceException");
  }
  catch (...) {
    xbt_test_assert(false, "Unexpected exception caught");
  }
}

XBT_TEST_UNIT("backtrace_nested", test_exception_backtrace_nested, "Throw an exception with a backtrace and a nested exception")
{
  xbt_test_add("Throw exception with backtrace and nested exception");
  try {
    try {
      simgrid::xbt::throw_with_backtrace(std::runtime_error(message));
    }
    catch (std::exception& e) {
      simgrid::xbt::throw_with_backtrace_and_nested(
        std::logic_error(message2));
    }
  }
  catch (std::exception& e) {
    xbt_test_assert(std::strcmp(message2, e.what()) == 0,
      "Same message");
    xbt_test_assert(instance_of<std::logic_error>(e),
      "Not a logic_error");
    xbt_test_assert(instance_of<std::nested_exception>(e),
      "Not a nested_exception");
    xbt_test_assert(instance_of<simgrid::xbt::BacktraceException>(e),
      "Not a BacktraceException");
    try {
      dynamic_cast<std::nested_exception&>(e).rethrow_nested();
    }
    catch(std::exception& ne) {
      xbt_test_assert(std::strcmp(message, ne.what()) == 0,
        "Same message");
      xbt_test_assert(instance_of<std::runtime_error>(ne),
        "Not a logic_error");
      xbt_test_assert(instance_of<simgrid::xbt::BacktraceException>(ne),
        "Not a BacktraceException");
    }
  }
  catch (...) {
    xbt_test_assert(false, "Unexpected exception caught");
  }
}

}

#endif                          /* SIMGRID_TEST */
