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
 * TimeZoneMenuItem
 *
 * 
 * Modification History:
 * $Log: TimeZoneMenuItem.java,v $
 * Revision 1.1  2000/08/19 14:35:58  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:11  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:18  polandj
 * Adding irrj under repository
 *
 * Revision 1.5  1999/03/12 06:02:48  vecna
 * CWindow.java:
 * Removed some commented out code.  Removed some of the sizing code that
 * wasn't doing anything particularly useful.  We still have the problem of
 * windows snapping to the top left corner, but at least the borders and
 * titlebar are visible.
 *
 * TimeZoneMenuItem.java:
 * I noticed that some of the TimeZones with a negative offset from GMT
 * were printing out as --<hour>...
 *
 * Revision 1.4  1999/03/04 22:11:50  sushila
 * Modification to support signing apps to be released
 *
 * Netscape privilege support added
 *
 * Revision 1.3  1999/02/03 22:43:55  vecna
 * Added more TimeZones.  Use if pretty self-explanatory...
 *
 * Revision 1.2  1999/02/02 07:33:25  vecna
 * Nuked some commented out code from DateMenu and TimeZoneMenu...
 *:
 *
 */

import java.awt.*;
import java.util.*;

import ipma.PlugIn.*;

public class TimeZoneMenuItem
   extends MenuItem {
   TimeZone TZ;
   
   public TimeZoneMenuItem(TimeZone newTZ) {
      StringBuffer   TZLabel;
      int   RawOffset;
      SimpleTimeZone STZ = (SimpleTimeZone ) newTZ;

      this.TZ = newTZ;
      RawOffset = STZ.getRawOffset();

      TZLabel = new StringBuffer();
      TZLabel.append(TZ.getID());
      TZLabel.append(" (");
      if (RawOffset >= 0)
         TZLabel.append('+');
      // RawOffset is in milliseconds
      TZLabel.append(RawOffset / (1000 * 60 * 60));
      TZLabel.append(')');
      setLabel(TZLabel.toString());
   }
   
   public TimeZone GetTZ() {
      return TZ;
   }
}
