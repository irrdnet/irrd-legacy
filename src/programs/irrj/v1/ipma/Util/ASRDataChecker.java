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
 * ASRDataChecker.
 * 
 * Verifies that a data packet contains AS number -> name resolution data,
 * and generates a ASRDataSet object from it.
 * 
 * @version $Revision: 1.1 $
 * @date    $Date: 2000/08/19 14:35:50 $
 * @author  quichem
 * @see     ipma.Util.ASRDataSet
 */

/*
 * Last modified by: $Author: polandj $
 * Modification History:
 *
 * $Log: ASRDataChecker.java,v $
 * Revision 1.1  2000/08/19 14:35:50  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:06  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:08  polandj
 * Adding irrj under repository
 *
 * Revision 1.7  1999/03/06 20:44:52  vecna
 * Switched internic:asname to internic_asname...
 *
 * Revision 1.6  1999/02/02 07:34:09  vecna
 * No changes...
 *
 * Revision 1.5  1999/01/29 11:59:17  vecna
 * Made a lot of formatting changes since J++ has an indent equivalent for Java...
 *
 * Revision 1.4  1998/12/20 01:24:09  dapengz
 * Old Copyright messages are replaced by the new one.
 *
 * Revision 1.3  1998/06/24 18:21:26  quichem
 * doc changes
 *
 *
 */


import ipma.PlugIn.*;
import ipma.Salamander.CSalamanderData;
import ipma.Wait.CWaitItem;

public class ASRDataChecker
implements CDataSetTypeCheckImpl {
  
  public static final String KEY  = "internic_asname";
  
  public final CDataSet  CreateDataSet(CSalamanderData SalyData, 
				       CWaitItem WaitItem) {
    return (CDataSet) new ASRDataSet(SalyData, WaitItem);
  }

  
  public final boolean CheckType(String Key) {
    return (Key.indexOf(KEY) == 0);
  }

}
