/* Copyright (c) 2010-2016. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "simgrid/msg.h"

XBT_LOG_NEW_DEFAULT_CATEGORY(msg_async_wait, "Messages specific for this msg example");

/* Sender process expects 6 arguments: */
static int sender(int argc, char *argv[])
{
  long number_of_tasks = xbt_str_parse_int(argv[1], "Invalid amount of tasks: %s");        /* - number of tasks */
  double task_comp_size = xbt_str_parse_double(argv[2], "Invalid computational size: %s"); /* - computational cost */
  double task_comm_size = xbt_str_parse_double(argv[3], "Invalid communication size: %s"); /* - communication cost */
  long receivers_count = xbt_str_parse_int(argv[4], "Invalid amount of receivers: %s");    /* - number of receivers */
  double sleep_start_time = xbt_str_parse_double(argv[5], "Invalid sleep start time: %s"); /* - start time */
  double sleep_test_time = xbt_str_parse_double(argv[6], "Invalid test time: %s");         /* - test time */

  XBT_INFO("sleep_start_time : %f , sleep_test_time : %f", sleep_start_time, sleep_test_time);

  int i;
  msg_task_t task = NULL;
  msg_comm_t comm = NULL;
  MSG_process_sleep(sleep_start_time);
  for (i = 0; i < number_of_tasks; i++) {
    char mailbox[256];
    char sprintf_buffer[256];

    sprintf(mailbox, "receiver-%ld", i % receivers_count);
    sprintf(sprintf_buffer, "Task_%d", i);

    /* This process first creates a task and send it asynchronously with @ref MSG_task_isend. Then, if: */
    task = MSG_task_create(sprintf_buffer, task_comp_size, task_comm_size, NULL);
    comm = MSG_task_isend(task, mailbox);
    XBT_INFO("Send to receiver-%ld Task_%d", i % receivers_count, i);

    if (sleep_test_time == 0) { /* - "test_time" is set to 0, wait on @ref MSG_comm_wait */
      MSG_comm_wait(comm, -1);
    } else {
      while (MSG_comm_test(comm) == 0) { /* - Call @ref MSG_comm_test every "test_time" otherwise */
        MSG_process_sleep(sleep_test_time);
      };
    }
    MSG_comm_destroy(comm);
  }

  for (i = 0; i < receivers_count; i++) {
    char mailbox[80];
    sprintf(mailbox, "receiver-%ld", i % receivers_count);
    task = MSG_task_create("finalize", 0, 0, 0);
    comm = MSG_task_isend(task, mailbox);
    XBT_INFO("Send to receiver-%ld finalize", i % receivers_count);
    if (sleep_test_time == 0) {
      MSG_comm_wait(comm, -1);
    } else {
      while (MSG_comm_test(comm) == 0) {
        MSG_process_sleep(sleep_test_time);
      };
    }
    MSG_comm_destroy(comm);
  }

  XBT_INFO("Goodbye now!");
  return 0;
}

/* Receiver process expects 3 arguments: */
static int receiver(int argc, char *argv[])
{
  msg_task_t task = NULL;
  XBT_ATTRIB_UNUSED msg_error_t res;
  char mailbox[80];
  msg_comm_t res_irecv;
  int id = xbt_str_parse_int(argv[1], "Invalid id: %s");                                        /* - unique id */
  double sleep_start_time = xbt_str_parse_double(argv[2], "Invalid sleep start parameter: %s"); /* - start time */
  double sleep_test_time = xbt_str_parse_double(argv[3], "Invalid sleep test parameter: %s");   /* - test time */
  XBT_INFO("sleep_start_time : %f , sleep_test_time : %f", sleep_start_time, sleep_test_time);

  MSG_process_sleep(sleep_start_time); /* This process first sleeps for "start time" seconds.  */

  sprintf(mailbox, "receiver-%d", id);
  while (1) {
    res_irecv = MSG_task_irecv(&(task), mailbox); /* Then it posts asynchronous receives (@ref MSG_task_irecv) and*/
    XBT_INFO("Wait to receive a task");

    if (sleep_test_time == 0) {               /* - if "test_time" is set to 0, wait on @ref MSG_comm_wait */
      res = MSG_comm_wait(res_irecv, -1);
      xbt_assert(res == MSG_OK, "MSG_task_get failed");
    } else {
      while (MSG_comm_test(res_irecv) == 0) { /* - Call @ref MSG_comm_test every "test_time" otherwise */
        MSG_process_sleep(sleep_test_time);
      };
    }
    MSG_comm_destroy(res_irecv);

    XBT_INFO("Received \"%s\"", MSG_task_get_name(task));
    if (!strcmp(MSG_task_get_name(task), "finalize")) { /* If the received task is "finalize", the process ends */
      MSG_task_destroy(task);
      break;
    }

    XBT_INFO("Processing \"%s\"", MSG_task_get_name(task)); /* Otherwise, the task is processed */
    MSG_task_execute(task);
    XBT_INFO("\"%s\" done", MSG_task_get_name(task));
    MSG_task_destroy(task);
    task = NULL;
  }
  XBT_INFO("I'm done. See you!");
  return 0;
}

int main(int argc, char *argv[])
{
  msg_error_t res = MSG_OK;

  MSG_init(&argc, argv);
  xbt_assert(argc > 2, "Usage: %s platform_file deployment_file\n"
             "\tExample: %s msg_platform.xml msg_deployment.xml\n", argv[0], argv[0]);

  MSG_create_environment(argv[1]);/* - Load the platform description */

  MSG_function_register("sender", sender);
  MSG_function_register("receiver", receiver);
  MSG_launch_application(argv[2]);/* - Deploy the sender and receiver processes */

  res = MSG_main();  /* - Run the simulation */

  XBT_INFO("Simulation time %g", MSG_get_clock());

  return res != MSG_OK;
}
