package irr;

/*
 * Copyright (c) 1997, 1998
 *      The Regents of the University of Michigan ("The Regents").
 *      All rights reserved.
 *
 * Contact: ipma-support@merit.edu
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      Michigan and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *    
 */   




/**
 * IRR
 * 
 * 
 * Modification History:
 *
 * Moved away from Salamander
 * @may 99
 * @author Jon Poland
 *
 * Port to jdk 1.1. Also combined applet, application and launch
 * modes into this one object.
 * @version 1.1.
 * @date 11 June 1998.
 * @author Sushila Subramanian.
 *
 * @version 1.0.
 * @date .
 * @author Aaron Klink.
 */

import java.applet.*;
import ipma.Util.*;

public final class IRRj extends Applet implements Runnable {
    // Set up defaults for the irr server and port
    static private String                irrServer = "whois.radb.net";
    static private int                   irrPort   = 43;

  
  /* Default Constructor, for applet.  irrServer, irrPort filled from above */
  public IRRj() {
    new Thread (this).start();  /* do nothing basically */
  }
  
  /* Constructor called when run from commandline */
  public IRRj (String Server, int Port){   
    irrServer = Server;
    irrPort = Port;
    new Thread(this).start();
  }
  
  /* Run when started as application */
  public static void main(String args[]){
    if(args.length == 2){
      new IRRj (args[0], Integer.valueOf(args[1]).intValue());
    }
    else{
      new IRRj(irrServer, irrPort);
    }
    
  }
  
  /* Run when started as an applet */
  public void run() {
    new IRRWindow(irrServer,irrPort);
  }
  
  /* problem is the run method runs before the init method, so this doesn't matter */
  public void init() {
    System.out.print("Init applet");
    String tempServer = getParameter("QSERVER");
    String tempPort = getParameter("QPORT");
    if(tempServer != null){
      irrServer = tempServer;
      System.out.print("Server PARAM != null");
    }
    if(tempPort != null){
      try{
	irrPort = Integer.valueOf(tempPort).intValue();
	System.out.print("Server PORT != null");
      }
      catch(NumberFormatException f){}
    }
    //new IRRj(irrServer, irrPort);
  }
  
} // End of irrj class definition

