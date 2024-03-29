/* Copyright (c) 2007-2010, 2012-2015. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef SIMGRID_SIMIX_HPP
#define SIMGRID_SIMIX_HPP

#include <cstddef>

#include <exception>
#include <string>
#include <utility>
#include <memory>
#include <functional>
#include <future>
#include <type_traits>

#include <xbt/function_types.h>
#include <simgrid/simix.h>

XBT_PUBLIC(void) simcall_run_kernel(std::function<void()> const& code);

namespace simgrid {
namespace simix {

/** Fulfill a promise by executing a given code */
template<class R, class F>
void fulfill_promise(std::promise<R>& promise, F&& code)
{
  try {
    promise.set_value(std::forward<F>(code)());
  }
  catch(...) {
    promise.set_exception(std::current_exception());
  }
}

/** Fulfill a promise by executing a given code
 *
 *  This is a special version for `std::promise<void>` because the default
 *  version does not compile in this case.
 */
template<class F>
void fulfill_promise(std::promise<void>& promise, F&& code)
{
  try {
    std::forward<F>(code)();
    promise.set_value();
  }
  catch(...) {
    promise.set_exception(std::current_exception());
  }
}

/** Execute some code in the kernel/maestro
 *
 *  This can be used to enforce mutual exclusion with other simcall.
 *  More importantly, this enforces a deterministic/reproducible ordering
 *  of the operation with respect to other simcalls.
 */
template<class F>
typename std::result_of<F()>::type kernel(F&& code)
{
  // If we are in the maestro, we take the fast path and execute the
  // code directly without simcall mashalling/unmarshalling/dispatch:
  if (SIMIX_is_maestro())
    return std::forward<F>(code)();

  // If we are in the application, pass the code to the maestro which is
  // executes it for us and reports the result. We use a std::future which
  // conveniently handles the success/failure value for us.
  typedef typename std::result_of<F()>::type R;
  std::promise<R> promise;
  simcall_run_kernel([&]{
    xbt_assert(SIMIX_is_maestro(), "Not in maestro");
    fulfill_promise(promise, std::forward<F>(code));
  });
  return promise.get_future().get();
}

class args {
private:
  int argc_ = 0;
  char** argv_ = nullptr;
public:

  // Main constructors
  args() {}

  void assign(int argc, const char*const* argv)
  {
    clear();
    char** new_argv = xbt_new(char*,argc + 1);
    for (int i = 0; i < argc; i++)
      new_argv[i] = xbt_strdup(argv[i]);
    new_argv[argc] = nullptr;
    this->argc_ = argc;
    this->argv_ = new_argv;
  }
  args(int argc, const char*const* argv)
  {
    this->assign(argc, argv);
  }

  char** to_argv() const
  {
    const int argc = argc_;
    char** argv = xbt_new(char*, argc + 1);
    for (int i=0; i< argc; i++)
      argv[i] = xbt_strdup(argv_[i]);
    argv[argc] = nullptr;
    return argv;
  }

  // Free
  void clear()
  {
    for (int i = 0; i < this->argc_; i++)
      free(this->argv_[i]);
    free(this->argv_);
    this->argc_ = 0;
    this->argv_ = nullptr;
  }
  ~args() { clear(); }

  // Copy
  args(args const& that)
  {
    this->assign(that.argc(), that.argv());
  }
  args& operator=(args const& that)
  {
    this->assign(that.argc(), that.argv());
    return *this;
  }

  // Move:
  args(args&& that) : argc_(that.argc_), argv_(that.argv_)
  {
    that.argc_ = 0;
    that.argv_ = nullptr;
  }
  args& operator=(args&& that)
  {
    this->argc_ = that.argc_;
    this->argv_ = that.argv_;
    that.argc_ = 0;
    that.argv_ = nullptr;
    return *this;
  }

  int    argc()            const { return argc_; }
  char** argv()                  { return argv_; }
  const char*const* argv() const { return argv_; }
  char* operator[](std::size_t i) { return argv_[i]; }
};

inline std::function<void()> wrap_main(
  xbt_main_func_t code,  std::shared_ptr<simgrid::simix::args> args)
{
  if (code) {
    return [=]() {
      code(args->argc(), args->argv());
    };
  }
  else return std::function<void()>();
}

inline
std::function<void()> wrap_main(xbt_main_func_t code, simgrid::simix::args args)
{
  if (code)
    return wrap_main(code, std::unique_ptr<simgrid::simix::args>(
      new simgrid::simix::args(std::move(args))));
  else return std::function<void()>();
}

inline
std::function<void()> wrap_main(xbt_main_func_t code, int argc, const char*const* argv)
{
  return wrap_main(code, simgrid::simix::args(argc, argv));
}

class Context;
class ContextFactory;

XBT_PUBLIC_CLASS ContextFactory {
private:
  std::string name_;
public:

  ContextFactory(std::string name) : name_(std::move(name)) {}
  virtual ~ContextFactory();
  virtual Context* create_context(std::function<void()> code,
    void_pfn_smxprocess_t cleanup, smx_process_t process) = 0;

  // Optional methods for attaching main() as a context:

  /** Creates a context from the current context of execution
   *
   *  This will not work on all implementation of `ContextFactory`.
   */
  virtual Context* attach(void_pfn_smxprocess_t cleanup_func, smx_process_t process);
  virtual Context* create_maestro(std::function<void()> code, smx_process_t process);

  virtual void run_all() = 0;
  virtual Context* self();
  std::string const& name() const
  {
    return name_;
  }
private:
  void declare_context(void* T, std::size_t size);
protected:
  template<class T, class... Args>
  T* new_context(Args&&... args)
  {
    T* context = new T(std::forward<Args>(args)...);
    this->declare_context(context, sizeof(T));
    return context;
  }
};

XBT_PUBLIC_CLASS Context {
private:
  std::function<void()> code_;
  void_pfn_smxprocess_t cleanup_func_ = nullptr;
  smx_process_t process_ = nullptr;
public:
  bool iwannadie;
public:
  Context(std::function<void()> code,
          void_pfn_smxprocess_t cleanup_func,
          smx_process_t process);
  void operator()()
  {
    code_();
  }
  bool has_code() const
  {
    return (bool) code_;
  }
  smx_process_t process()
  {
    return this->process_;
  }
  void set_cleanup(void_pfn_smxprocess_t cleanup)
  {
    cleanup_func_ = cleanup;
  }

  // Virtual methods
  virtual ~Context();
  virtual void stop();
  virtual void suspend() = 0;
};

XBT_PUBLIC_CLASS AttachContext : public Context {
public:

  AttachContext(std::function<void()> code,
          void_pfn_smxprocess_t cleanup_func,
          smx_process_t process)
    : Context(std::move(code), cleanup_func, process)
  {}

  ~AttachContext();

  /** Called by the context when it is ready to give control
   *  to the maestro.
   */
  virtual void attach_start() = 0;

  /** Called by the context when it has finished its job */
  virtual void attach_stop() = 0;
};

XBT_PUBLIC(void) set_maestro(std::function<void()> code);
XBT_PUBLIC(void) create_maestro(std::function<void()> code);

}
}

/*
 * Type of function that creates a process.
 * The function must accept the following parameters:
 * void* process: the process created will be stored there
 * const char *name: a name for the object. It is for user-level information and can be NULL
 * xbt_main_func_t code: is a function describing the behavior of the process
 * void *data: data a pointer to any data one may want to attach to the new object.
 * sg_host_t host: the location where the new process is executed
 * int argc, char **argv: parameters passed to code
 * xbt_dict_t pros: properties
 */
typedef smx_process_t (*smx_creation_func_t) (
                                      /* name */ const char*,
                                      std::function<void()> code,
                                      /* userdata */ void*,
                                      /* hostname */ const char*,
                                      /* kill_time */ double,
                                      /* props */ xbt_dict_t,
                                      /* auto_restart */ int,
                                      /* parent_process */ smx_process_t);

extern "C"
XBT_PUBLIC(void) SIMIX_function_register_process_create(smx_creation_func_t function);

XBT_PUBLIC(smx_process_t) simcall_process_create(const char *name,
                                          std::function<void()> code,
                                          void *data,
                                          const char *hostname,
                                          double kill_time,
                                          xbt_dict_t properties,
                                          int auto_restart);

XBT_PUBLIC(smx_timer_t) SIMIX_timer_set(double date, std::function<void()> callback);

template<class R, class T> inline
XBT_PUBLIC(smx_timer_t) SIMIX_timer_set(double date, R(*callback)(T*), T* arg)
{
  return SIMIX_timer_set(date, [=](){ callback(arg); });
}

#endif
