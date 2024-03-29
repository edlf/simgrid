/* Copyright (c) 2012-2014, 2016. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

package process.migration;
import org.simgrid.msg.Msg;
import org.simgrid.msg.Mutex;
import org.simgrid.msg.Process;
import org.simgrid.msg.NativeException;
import org.simgrid.msg.HostNotFoundException;

public class Main {
  public static Mutex mutex;
  public static Process processToMigrate = null;

  public static void main(String[] args) throws NativeException {
    Msg.init(args);
    if(args.length < 1) {
      Msg.info("Usage   : Migration platform_file");
      Msg.info("example : Migration ../platforms/platform.xml");
      System.exit(1);
    }
    /* Create the mutex */
    mutex = new Mutex();

    /* construct the platform*/
    Msg.createEnvironment(args[0]);
    /* bypass deploymemt */
    try {
        Policeman policeman = new Policeman("Boivin","policeman");
        policeman.start();
        Emigrant emigrant   = new Emigrant("Jacquelin","emigrant");
        emigrant.start();
    } catch (HostNotFoundException e){
      System.out.println("Create processes failed!");
    }

    /*  execute the simulation. */
    Msg.run();
  }
}
