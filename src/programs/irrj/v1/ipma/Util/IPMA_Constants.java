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
 * IPMA_Constants
 * 
 * An interface consisting of a bunch of constants common 
 * across all tools. Any tool that wants to access these 
 * constants just has to implement this interface
 * 
 * @version 1.0
 * @date  20 May 1998
 * @author Sushila Subramanian 
 */

/*
 * Last modified by: $Author: polandj $
 * Modification History:
 *  
 * $Log: IPMA_Constants.java,v $
 * Revision 1.1  2000/08/19 14:35:53  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:07  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:11  polandj
 * Adding irrj under repository
 *
 * Revision 1.13  1999/03/10 22:37:34  dapengz
 * Formatting change.  No code modifications
 *
 * Revision 1.12  1999/01/29 22:26:49  dapengz
 * Modified so that they work with the new HTTP interface
 *
 * Revision 1.11  1998/12/20 01:24:12  dapengz
 * Old Copyright messages are replaced by the new one.
 *
 * Revision 1.10  1998/07/17 15:34:35  aklink
 * Changed the default IRR server to radb1.merit.edu
 *
 * 	Aaron
 *
 * Revision 1.9  1998/07/17 15:17:30  aklink
 * Changed the default irr port to 43.
 *
 * 	Aaron
 *
 * Revision 1.8  1998/07/17 03:50:39  quichem
 * - added a String for default channel preference name
 *
 */   

public interface IPMA_Constants {

  // Default server/port
  public static final String SALY_SERVER = "salamander.merit.edu";
  public static final int SALY_PORT = 9000;

  public static final String PREF_SALY_URL    = "SalamanderURL";
/*
  public static final String TOOLLIST_CHANNEL = "ToolList:IPMA";
*/
  public static final String NEW_TOOLLIST_CHANNEL = "ToolList:IPMA1.1";

  // Default server for IRR
  public static final String IRR_SERVER = "radb1.merit.edu";
  public static final int IRR_PORT = 43;

  // Default server for RouteTracker
  public static final String ROUTE_TRACKER_SERVER = "zounds.merit.net";
  public static final int ROUTE_TRACKER_PORT = 5670; 

  // names for the global preferences
  public static final String PREF_SALAMANDER_SERVER = "SalServer";
  public static final String PREF_SALAMANDER_PORT =  "SalPort";
  public static final String PREF_DEBUG = "DebugLevel";
  public static final String PREF_DEF_CHANNEL = "DefaultChannel";
  
  // IPMA Tools - this is needed for translation - may not be
  // be necessary in the future.

  public static final String ROUTE_TRACKER = "RouteTracker";
  public static final String ROUTE_TABLE = "RouteTable";
  public static final String FLAP_TABLE = "FlapTable";
  public static final String FLAP_GRAPH = "FlapGraph";
  public static final String IRR  = "IRR";
  public static final String IPN  = "IPN";
  public static final String NETNOW_GRAPH = "NetNowGraph";
  public static final String NETNOW_LOSS_GRAPH = "NetNowLossMultiGraph";
  public static final String ASEXPLORER = "ASExplorer";
  public static final String SNMP_TABLE = "SNMPTable";
  public static final String TREND_GRAPH = "TrendFlapGraph";



}
