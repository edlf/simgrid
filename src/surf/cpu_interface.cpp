/* Copyright (c) 2013-2015. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <xbt/dynar.h>
#include "cpu_interface.hpp"
#include "plugins/energy.hpp"
#include "src/instr/instr_private.h" // TRACE_is_enabled(). FIXME: remove by subscribing tracing to the surf signals

XBT_LOG_EXTERNAL_CATEGORY(surf_kernel);
XBT_LOG_NEW_DEFAULT_SUBCATEGORY(surf_cpu, surf, "Logging specific to the SURF cpu module");

void_f_void_t surf_cpu_model_init_preparse = NULL;

simgrid::surf::CpuModel *surf_cpu_model_pm;
simgrid::surf::CpuModel *surf_cpu_model_vm;

namespace simgrid {
namespace surf {

/*************
 * Callbacks *
 *************/

simgrid::xbt::signal<void(CpuAction*, Action::State, Action::State)> cpuActionStateChangedCallbacks;

/*********
 * Model *
 *********/

void CpuModel::updateActionsStateLazy(double now, double /*delta*/)
{
  CpuAction *action;
  while ((xbt_heap_size(getActionHeap()) > 0)
         && (double_equals(xbt_heap_maxkey(getActionHeap()), now, sg_surf_precision))) {
    action = static_cast<CpuAction*>(xbt_heap_pop(getActionHeap()));
    XBT_CDEBUG(surf_kernel, "Something happened to action %p", action);
    if (TRACE_is_enabled()) {
      Cpu *cpu = static_cast<Cpu*>(lmm_constraint_id(lmm_get_cnst_from_var(getMaxminSystem(), action->getVariable(), 0)));
      TRACE_surf_host_set_utilization(cpu->getName(), action->getCategory(),
                                      lmm_variable_getvalue(action->getVariable()),
                                      action->getLastUpdate(),
                                      now - action->getLastUpdate());
    }

    action->finish();
    XBT_CDEBUG(surf_kernel, "Action %p finished", action);

    /* set the remains to 0 due to precision problems when updating the remaining amount */
    action->setRemains(0);
    action->setState(Action::State::done);
    action->heapRemove(getActionHeap()); //FIXME: strange call since action was already popped
  }
  if (TRACE_is_enabled()) {
    //defining the last timestamp that we can safely dump to trace file
    //without losing the event ascending order (considering all CPU's)
    double smaller = -1;
    ActionList *actionSet = getRunningActionSet();
    for(ActionList::iterator it(actionSet->begin()), itend(actionSet->end())
       ; it != itend ; ++it) {
      action = static_cast<CpuAction*>(&*it);
        if (smaller < 0) {
          smaller = action->getLastUpdate();
          continue;
        }
        if (action->getLastUpdate() < smaller) {
          smaller = action->getLastUpdate();
        }
    }
    if (smaller > 0) {
      TRACE_last_timestamp_to_dump = smaller;
    }
  }
  return;
}

void CpuModel::updateActionsStateFull(double now, double delta)
{
  CpuAction *action = NULL;
  ActionList *running_actions = getRunningActionSet();

  for(ActionList::iterator it(running_actions->begin()), itNext=it, itend(running_actions->end())
     ; it != itend ; it=itNext) {
    ++itNext;
    action = static_cast<CpuAction*>(&*it);
    if (TRACE_is_enabled()) {
      Cpu *cpu = static_cast<Cpu*> (lmm_constraint_id(lmm_get_cnst_from_var(getMaxminSystem(), action->getVariable(), 0)) );

      TRACE_surf_host_set_utilization(cpu->getName(),
                                      action->getCategory(),
                                      lmm_variable_getvalue(action->getVariable()),
                                      now - delta,
                                      delta);
      TRACE_last_timestamp_to_dump = now - delta;
    }

    action->updateRemains(lmm_variable_getvalue(action->getVariable()) * delta);


    if (action->getMaxDuration() != NO_MAX_DURATION)
      action->updateMaxDuration(delta);


    if ((action->getRemainsNoUpdate() <= 0) &&
        (lmm_get_variable_weight(action->getVariable()) > 0)) {
      action->finish();
      action->setState(Action::State::done);
    } else if ((action->getMaxDuration() != NO_MAX_DURATION) &&
               (action->getMaxDuration() <= 0)) {
      action->finish();
      action->setState(Action::State::done);
    }
  }
}

/************
 * Resource *
 ************/
Cpu::Cpu(Model *model, simgrid::s4u::Host *host, xbt_dynar_t speedPerPstate, int core)
 : Cpu(model, host, NULL/*constraint*/, speedPerPstate, core)
{
}

Cpu::Cpu(Model *model, simgrid::s4u::Host *host, lmm_constraint_t constraint,
    xbt_dynar_t speedPerPstate, int core)
 : Resource(model, host->name().c_str(), constraint)
 , coresAmount_(core)
 , host_(host)
{
  xbt_assert(core > 0, "Host %s must have at least one core, not 0.", host->name().c_str());

  speed_.peak = xbt_dynar_get_as(speedPerPstate, 0/*pstate*/, double);
  speed_.scale = 1;
  host->pimpl_cpu = this;
  xbt_assert(speed_.scale > 0, "Speed of host %s must be >0", host->name().c_str());

  // Copy the power peak array:
  speedPerPstate_ = xbt_dynar_new(sizeof(double), nullptr);
  unsigned long n = xbt_dynar_length(speedPerPstate);
  for (unsigned long i = 0; i != n; ++i) {
    double value = xbt_dynar_get_as(speedPerPstate, i, double);
    xbt_dynar_push(speedPerPstate_, &value);
  }

  xbt_assert(model == surf_cpu_model_pm || core==1, "Currently, VM cannot be multicore");

  if (model->getUpdateMechanism() != UM_UNDEFINED) {
    p_constraintCore = xbt_new(lmm_constraint_t, core);
    p_constraintCoreId = xbt_new(void*, core);

    for (int i = 0; i < core; i++) {
      /* just for a unique id, never used as a string. */
      p_constraintCoreId[i] = bprintf("%s:%i", host->name().c_str(), i);
      p_constraintCore[i] = lmm_constraint_new(model->getMaxminSystem(), p_constraintCoreId[i], speed_.scale * speed_.peak);
    }
  }
}

Cpu::~Cpu()
{
  if (p_constraintCoreId){
    for (int i = 0; i < coresAmount_; i++)
      xbt_free(p_constraintCoreId[i]);
    xbt_free(p_constraintCore);
  }
  xbt_free(p_constraintCoreId);
  xbt_dynar_free(&speedPerPstate_);
}

double Cpu::getCurrentPowerPeak()
{
  return speed_.peak;
}

int Cpu::getNbPStates()
{
  return xbt_dynar_length(speedPerPstate_);
}

void Cpu::setPState(int pstate_index)
{
  xbt_dynar_t plist = speedPerPstate_;
  xbt_assert(pstate_index <= (int)xbt_dynar_length(plist),
      "Invalid parameters for CPU %s (pstate %d > length of pstates %d)", getName(), pstate_index, (int)xbt_dynar_length(plist));

  double new_peak_speed = xbt_dynar_get_as(plist, pstate_index, double);
  pstate_ = pstate_index;
  speed_.peak = new_peak_speed;

  onSpeedChange();
}

int Cpu::getPState()
{
  return pstate_;
}

double Cpu::getPowerPeakAt(int pstate_index)
{
  xbt_dynar_t plist = speedPerPstate_;
  xbt_assert((pstate_index <= (int)xbt_dynar_length(plist)), "Invalid parameters (pstate index out of bounds)");

  return xbt_dynar_get_as(plist, pstate_index, double);
}

double Cpu::getSpeed(double load)
{
  return load * speed_.peak;
}

void Cpu::setSpeed(double sp){
  speed_.peak = sp;
  lmm_update_constraint_bound(getModel()->getMaxminSystem(), getConstraint(), speed_.peak * speed_.scale);
  onSpeedChange();
}

double Cpu::getAvailableSpeed()
{
/* number between 0 and 1 */
  return speed_.scale;
}

void Cpu::onSpeedChange() {
  TRACE_surf_host_set_speed(surf_get_clock(), getName(), coresAmount_ * speed_.scale * speed_.peak);
}


int Cpu::getCore()
{
  return coresAmount_;
}

void Cpu::setStateTrace(tmgr_trace_t trace)
{
  xbt_assert(stateEvent_==NULL,"Cannot set a second state trace to Host %s", host_->name().c_str());

  stateEvent_ = future_evt_set->add_trace(trace, 0.0, this);
}
void Cpu::setSpeedTrace(tmgr_trace_t trace)
{
  xbt_assert(speed_.event==NULL,"Cannot set a second speed trace to Host %s", host_->name().c_str());

  speed_.event = future_evt_set->add_trace(trace, 0.0, this);
}


/**********
 * Action *
 **********/

void CpuAction::updateRemainingLazy(double now)
{
  xbt_assert(getStateSet() == getModel()->getRunningActionSet(), "You're updating an action that is not running.");
  xbt_assert(getPriority() > 0, "You're updating an action that seems suspended.");

  double delta = now - lastUpdate_;

  if (remains_ > 0) {
    XBT_CDEBUG(surf_kernel, "Updating action(%p): remains was %f, last_update was: %f", this, remains_, lastUpdate_);
    double_update(&(remains_), lastValue_ * delta, sg_maxmin_precision*sg_surf_precision);

    if (TRACE_is_enabled()) {
      Cpu *cpu = static_cast<Cpu*>(lmm_constraint_id(lmm_get_cnst_from_var(getModel()->getMaxminSystem(), getVariable(), 0)));
      TRACE_surf_host_set_utilization(cpu->getName(), getCategory(), lastValue_, lastUpdate_, now - lastUpdate_);
    }
    XBT_CDEBUG(surf_kernel, "Updating action(%p): remains is now %f", this, remains_);
  }

  lastUpdate_ = now;
  lastValue_ = lmm_variable_getvalue(getVariable());
}

/*
 *
 * This function formulates a constraint problem that pins a given task to
 * particular cores. Currently, it is possible to pin a task to an exactly one
 * specific core. The system links the variable object of the task to the
 * per-core constraint object.
 *
 * But, the taskset command on Linux takes a mask value specifying a CPU
 * affinity setting of a given task. If the mask value is 0x03, the given task
 * will be executed on the first core (CPU0) or the second core (CPU1) on the
 * given PM. The schedular will determine appropriate placements of tasks,
 * considering given CPU affinities and task activities.
 *
 * How should the system formulate constraint problems for an affinity to
 * multiple cores?
 *
 * The cpu argument must be the host where the task is being executed. The
 * action object does not have the information about the location where the
 * action is being executed.
 */
void CpuAction::setAffinity(Cpu *cpu, unsigned long mask)
{
  lmm_variable_t var_obj = getVariable();
  XBT_IN("(%p,%lx)", this, mask);

  {
    unsigned long nbits = 0;

    /* FIXME: There is much faster algorithms doing this. */
    for (int i = 0; i < cpu->coresAmount_; i++) {
      unsigned long has_affinity = (1UL << i) & mask;
      if (has_affinity)
        nbits += 1;
    }

    xbt_assert(nbits <= 1, "Affinity mask cannot span over multiple cores.");
  }

  for (int i = 0; i < cpu->coresAmount_; i++) {
    XBT_DEBUG("clear affinity %p to cpu-%d@%s", this, i,  cpu->getName());
    lmm_shrink(cpu->getModel()->getMaxminSystem(), cpu->p_constraintCore[i], var_obj);

    unsigned long has_affinity = (1UL << i) & mask;
    if (has_affinity) {
      /* This function only accepts an affinity setting on the host where the
       * task is now running. In future, a task might move to another host.
       * But, at this moment, this function cannot take an affinity setting on
       * that future host.
       *
       * It might be possible to extend the code to allow this function to
       * accept affinity settings on a future host. We might be able to assign
       * zero to elem->value to maintain such inactive affinity settings in the
       * system. But, this will make the system complex. */
      XBT_DEBUG("set affinity %p to cpu-%d@%s", this, i, cpu->getName());
      lmm_expand(cpu->getModel()->getMaxminSystem(), cpu->p_constraintCore[i], var_obj, 1.0);
    }
  }

  if (cpu->getModel()->getUpdateMechanism() == UM_LAZY) {
    /* FIXME (hypervisor): Do we need to do something for the LAZY mode? */
  }
  XBT_OUT();
}

simgrid::xbt::signal<void(simgrid::surf::CpuAction*, Action::State)> CpuAction::onStateChange;

void CpuAction::setState(Action::State state){
  Action::State previous = getState();
  Action::setState(state);
  onStateChange(this, previous);
}
std::list<Cpu*> CpuAction::cpus() {
  std::list<Cpu*> retlist;
  lmm_system_t sys = getModel()->getMaxminSystem();
  int llen = lmm_get_number_of_cnst_from_var(sys, getVariable());

  for(int i = 0; i<llen; i++)
    retlist.push_back( (Cpu*)(lmm_constraint_id( lmm_get_cnst_from_var(sys, getVariable(), i) )) );

  return retlist;
}

}
}
