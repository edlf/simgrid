/* Copyright (c) 2006-2014, 2016. The SimGrid Team.
 * All rights reserved.                                                     */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

package app.pingpong;
import org.simgrid.msg.Msg;
import org.simgrid.msg.MsgException;
import org.simgrid.msg.NativeException;
 
public class Main {
  public static void main(String[] args) throws MsgException, NativeException{
    Msg.init(args);
    if(args.length < 1) {
      Msg.info("Usage   : Main platform_file");
      Msg.info("example : Main ../platforms/platform.xml");
      System.exit(1);
    }

    Msg.createEnvironment(args[0]);
    new Sender("Jacquelin", "Sender", new String[] {"Boivin"}).start();
    new Receiver ("Boivin", "Receiver", null).start();

    Msg.run();
  }
}
