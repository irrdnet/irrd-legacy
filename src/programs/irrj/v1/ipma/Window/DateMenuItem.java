package ipma.Window;

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
 * DateMenuItem
 *
 * <functionality>
 * 
 * Modification History:
 * 
 * $Log: DateMenuItem.java,v $
 * Revision 1.1  2000/08/19 14:35:57  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:10  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:16  polandj
 * Adding irrj under repository
 *
 * Revision 1.6  1999/03/11 05:31:58  vecna
 * Removed '@' at users' request...
 *
 * Revision 1.5  1999/02/02 07:33:24  vecna
 * Nuked some commented out code from DateMenu and TimeZoneMenu...
 *:
 *
 */
import java.awt.*;
import java.util.*;

public class DateMenuItem extends MenuItem {
   public final static String	DATE_SEPARATOR = "/";
   Calendar	Cal;
   DateMenuItem(Calendar Cal) {
      StringBuffer	LabelBuffer;
      int	Year,
         Month,
         Day;

      this.Cal		= Cal;
      Year			= Cal.get(Calendar.YEAR);
      Month			= Cal.get(Calendar.MONTH) + 1;
      Day			= Cal.get(Calendar.DATE);
      LabelBuffer	= new StringBuffer();
//      LabelBuffer.append('@');
      LabelBuffer.append(Integer.toString(Year));
      LabelBuffer.append(DATE_SEPARATOR);
      if (Month < 10)
         LabelBuffer.append('0');

      LabelBuffer.append(Integer.toString(Month));
      LabelBuffer.append(DATE_SEPARATOR);

      if (Day < 10)
         LabelBuffer.append('0');

      LabelBuffer.append(Integer.toString(Day));
      setLabel(LabelBuffer.toString());
   }
   public Calendar GetCal() {
      return Cal;
   }
}
