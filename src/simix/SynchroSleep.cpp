/* Copyright (c) 2007-2016. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "src/simix/SynchroSleep.hpp"
#include "src/surf/surf_interface.hpp"

void simgrid::simix::Sleep::suspend() {
  surf_sleep->suspend();
}

void simgrid::simix::Sleep::resume() {
  surf_sleep->resume();
}