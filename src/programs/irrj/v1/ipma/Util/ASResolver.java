package ipma.Util;

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
 * ASResolver.
 * 
 * A utility class for resolving AS names from numbers.
 * Also usable as a standalone application.
 */

/*
 * Last modified by: $Author: polandj $
 * Modification History:
 *
 * $Log: ASResolver.java,v $
 * Revision 1.1  2000/08/19 14:35:51  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:06  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:09  polandj
 * Adding irrj under repository
 *
 * Revision 1.11  1999/03/06 20:44:52  vecna
 * Switched internic:asname to internic_asname...
 *
 * Revision 1.10  1999/01/29 22:26:49  dapengz
 * Modified so that they work with the new HTTP interface
 *
 * Revision 1.9  1999/01/29 11:59:18  vecna
 * Made a lot of formatting changes since J++ has an indent equivalent for Java...
 *
 * Revision 1.8  1998/12/20 01:24:10  dapengz
 * Old Copyright messages are replaced by the new one.
 *
 * Revision 1.7  1998/11/15 08:35:44  vecna
 * Bug in ASResolver where a Null Pointer Exception would pop.  This is a
 * quick hack to resolve the situation, by simply catching the exception
 * and returning a null value...
 *
 * Revision 1.6  1998/11/14 18:13:41  vecna
 * Found another place where this bombed with a Null Pointer Exception.
 * Fixed with a simple try/catch...  Should be handled elsewhere...
 *
 * Revision 1.5  1998/11/02 23:06:26  dapengz
 * ASResolver.resolve is added to be called by other classes to resolve AS numbers to names
 *
 * Revision 1.4  1998/07/07 01:39:51  quichem
 * bugfix: now sends output to System.out when invoked as standalone
 *
 * Revision 1.3  1998/06/24 18:21:26  quichem
 * doc changes
 *
 *
 */


import java.net.*;
import java.util.*;

import ipma.PlugIn.CDataCollection;
import ipma.Query.*;
import ipma.Salamander.*;

import debug.*;

public class ASResolver {
   
   // why make all this junk static?
   //
   // if we made it a regular class, then each applet that used it
   // would create a new instance. i think that's pretty wasteful.
   //
   // we could, instead, pass an instance of ASResolver to everything,
   // but not everything is going to need it, and i don't want to
   // clutter up the interfaces.
   //
   // i thought about using the singleton pattern, but apparently, there
   // are some problems with it unless using Java 1.2. (the runtime environment
   // is permitted to unload classes that aren't currently referenced.)
   //
   // --mukesh
   
   static final String USAGE_STRING = "usage: <AS-number> [server] [port]";
   static final String IO_ERROR_STRING = "A communications error occurred.";
   static final String BAD_PORT_STRING = "Invalid Port Number";
   static final int    ERROR_BADARGS = 1;
   
   static final String SERVER = "salamander.merit.edu";
   static final int    PORT   = 9000;
   static final String RESOLVER_CHANNEL = "internic_asname";
   
   static boolean ready = false;
   static CSalamanderImpl salServer;
   static CSalamanderListener salListen;
   static QuerySubmitter querySubmitter;
   static ResponseHandler salHandler;
   static ASRDataSet asInfo;
   
   public static void init(CSalamanderImpl salServ, CSalamanderListener salList,
      QuerySubmitter querySub, ResponseHandler salHand) {
      if (!ready) {
         salServer = salServ;
         salListen = salList;
         querySubmitter = querySub;
         salHandler = salHand;
         // i'm assuming that no one else uses ASRDataSets.
         // this should probably be changed
         salHandler.RegisterTypeChecker(new ASRDataChecker());
         doQuery();
      }
   }
   
   public static void refresh() {
      doQuery();
   }
   
   public static String getName(int asNum) {
      String ASName;
      // Jimmy - This is a quick and dirty hack.  There should be
      // a null pointer check somewhere else down the line...
      try {
         ASName = (String)asInfo.nameByNumber.get(new Integer(asNum));
      }
      catch (NullPointerException NPE) {
         ASName = null;
      }
      return ASName;
   }
   
   protected static void doQuery() {
      Query q;
      
      // no title, no properties, not persistent, don't attempt to link
      // into an existing query, and don't let others link into this one
      q = new Query(RESOLVER_CHANNEL, null, null, false, false, false);
      querySubmitter.SubmitQuery(q);

      // This takes forever, lets make it timeout after 10 seconds
      if (salHandler.WaitForReady(q, 10000)) {
         CDataCollection c;      

         c = q.getCollection();
         
         try {
            asInfo = (ASRDataSet) c.GetNewestDataSet();
            ready = true;
            return;
         } catch (ClassCastException CCE) {
         }
      }

      // either not ready, or cce happend
      ready = false;
   }
   
   public static void main(String Args[])
      throws java.io.IOException {
      
      String   Server;
      String   PortString;
      int      PortNum;
      int      ASnum;
      String   ASName;
      String   buffer;
      int      length = Args.length;
      
      Server   = SERVER;
      PortNum  = PORT;
      
      if (length < 1) {
         Debug.print(Debug.ERR, USAGE_STRING);
         System.exit(ERROR_BADARGS);
      }
      
      try {
         ASnum = Integer.parseInt(Args[0]);
         
         if (length >= 3) {
            PortString = Args[2];
            try {
               PortNum = Integer.parseInt(PortString);
            }
            catch (NumberFormatException NFE) {
               Debug.print(Debug.ERR, BAD_PORT_STRING);
               System.exit(0);
            }
         }
         if (length >= 2)
            Server = Args[1];
         
         CSalamander saly = new CSalamander(Server, PortNum);
         ResponseHandler rh = new ResponseHandler();
         CSalamanderListener sl = new CSalamanderListener(saly, rh, true);
         QuerySubmitter qs = new QuerySubmitter(saly, rh);
         
         saly.Connect();
         sl.StartListening();
         
         init(saly, sl, qs, rh);
         // System.out.println(getName(ASnum));

         ASName = getName(ASnum);
         if (ASName == null)
            System.out.println("ASName not found");
         else
            System.out.println(ASName);
         
         sl.StopListening();
         saly.Disconnect();
      } catch (NumberFormatException NFE) {
         Debug.print(Debug.ERR, USAGE_STRING);
         System.exit(ERROR_BADARGS);
      }
   }

   public static String resolve(String ASNum){
      
      if (!ASNum.startsWith("AS")){
         return ASNum;
      }

      int asNum = 0;

      try{
         asNum = Integer.parseInt(ASNum.substring(2));
      }
      catch(NumberFormatException exception){
         System.err.println("invalid AS number passed into ASResolver.resolve");
         return ASNum;
      }

      CSalamander saly = new CSalamander(SERVER, PORT);

      ResponseHandler rh = new ResponseHandler();

      CSalamanderListener sl = new CSalamanderListener(saly, rh, true);

      QuerySubmitter qs = new QuerySubmitter(saly, rh);

      saly.Connect();
      sl.StartListening();

      init(saly, sl, qs, rh);
      String returnValue = getName(asNum);

      sl.StopListening();

      saly.Disconnect();

      //System.out.println("returnValue is: " + returnValue);

      if (returnValue == null){
         returnValue = ASNum;
      }

      return returnValue;
   }
} 

