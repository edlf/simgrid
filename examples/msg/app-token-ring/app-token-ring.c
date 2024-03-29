/* Copyright (c) 2008-2016. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "simgrid/msg.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(msg_app_token_ring, "Messages specific for this msg example");

/* Main function of all processes used in this example */
static int foo(int argc, char *argv[])
{
  unsigned int task_comm_size = 1000000; /* The token is 1MB long*/
  int rank = xbt_str_parse_int(MSG_process_get_name(MSG_process_self()), "Any process of this example must have a numerical name, not %s");
  char mailbox[256];
  msg_task_t task = NULL;
  XBT_ATTRIB_UNUSED int res;
  if (rank == 0){ /* - The root (rank 0) first sends the token then waits to receive it back */
    sprintf(mailbox, "%d", rank+1);
    task = MSG_task_create("Token", 0, task_comm_size, NULL);
    XBT_INFO("Host \"%d\" send '%s' to Host \"%s\"", rank, task->name,mailbox);
    MSG_task_send(task, mailbox);
    task = NULL;
    res = MSG_task_receive(&(task), MSG_process_get_name(MSG_process_self()));
    xbt_assert(res == MSG_OK, "MSG_task_get failed");
    XBT_INFO("Host \"%d\" received \"%s\"", rank, MSG_task_get_name(task));
    MSG_task_destroy(task);
  } else{ /* - The others receive from their left neighbor (rank-1) and send to their right neighbor (rank+1) */
    res = MSG_task_receive(&(task), MSG_process_get_name(MSG_process_self()));
    xbt_assert(res == MSG_OK, "MSG_task_get failed");
    XBT_INFO("Host \"%d\" received \"%s\"",rank, MSG_task_get_name(task));

    if(rank+1 == MSG_get_host_number()) /* - Except for the last one which sends the token back to rank 0 */
      sprintf(mailbox, "0");
    else
      sprintf(mailbox, "%d", rank+1);
    XBT_INFO("Host \"%d\" send '%s' to Host \"%s\"",rank,task->name,mailbox);
    MSG_task_send(task, mailbox);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  unsigned int i;
  MSG_init(&argc, argv);
  MSG_create_environment(argv[1]);       /* - Load the platform description */
  xbt_dynar_t hosts = MSG_hosts_as_dynar();
  msg_host_t h;

  XBT_INFO("Number of host '%d'",MSG_get_host_number());
  xbt_dynar_foreach (hosts, i, h){      /* - Give a unique rank to each host and create a @ref foo process on each */
    char* name_host = bprintf("%u",i);
    MSG_process_create(name_host, foo, NULL, h);
    free(name_host);
  }
  xbt_dynar_free(&hosts);

  int res = MSG_main();                 /* - Run the simulation */
  XBT_INFO("Simulation time %g", MSG_get_clock());
  return res != MSG_OK;
}
