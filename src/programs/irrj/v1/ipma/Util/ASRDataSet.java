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
 * ASRDataSet
 * 
 * Provides structured access to AS name/number mapping data.
 * 
 * @version $Revision: 1.1 $
 * @date    $Date: 2000/08/19 14:35:51 $
 * @author  quichem
 */

/*
 * Last modified by: $Author: polandj $
 * Modification History:
 *
 * $Log: ASRDataSet.java,v $
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
 * Revision 1.5  1998/12/20 01:24:10  dapengz
 * Old Copyright messages are replaced by the new one.
 *
 * Revision 1.4  1998/06/24 19:16:05  quichem
 * zapping a typo
 *
 * Revision 1.3  1998/06/24 18:21:26  quichem
 * doc changes
 *
 *
 */


import java.util.*;

import ipma.PlugIn.CDataSet;
import ipma.Salamander.CSalamanderData;
import ipma.Wait.CWaitItem;

/*package*/ class ASRDataSet
extends CDataSet {
  
  protected static final String LINE_SEPARATOR = "\n";
  
  Hashtable nameByNumber;
  
  /*package*/ ASRDataSet(CSalamanderData SalyData,
			 CWaitItem WaitItem) {
    // so long as we're only using this for the ASResolver utility class,
    // we don't really need to check properties.
    //   CheckProperties(SalyData);
    nameByNumber = new Hashtable();
    ParseData(SalyData);
  }
  
  protected void ParseData(CSalamanderData salyData) {
    StringTokenizer   parser;
    String            rawData;
    String            asNumber, asName;
    Integer           asNum;
    
    rawData = new String(salyData.DataBuffer);
    parser  = new StringTokenizer(rawData, LINE_SEPARATOR);
    
    // data looks like...
    //    #1
    //    BBNPLANET
    //    JC347
    //    #3
    //    MIT-GATEWAYS
    //    RH164
    //    #<as-number>
    //    <as-name>
    //    <description?>
    
    while (parser.hasMoreTokens()) {
      try {
	asNumber = parser.nextToken();
	asName   = parser.nextToken();
	parser.nextToken();     // ignore description info
	
	asNum = Integer.valueOf(asNumber.substring(1));
	
	nameByNumber.put(asNum, asName);      
      } catch (NumberFormatException NFE) {
	// hmm... the line didn't contain an AS number, as we expected
	// it to. we'll just catch the exception and try again. (eventually,
	// we'll either find another entry, or just hit the end of the data)
      } catch (NoSuchElementException NSEE) {
	// the data ended prematurely (in the middle of an as entry.)
	// nothing we can do about it.
      }
    }
  }

}
