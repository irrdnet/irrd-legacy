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
 * DateMenu
 * 
 * <functionality> ??
 * 
 * Modification History:
 * 
 * $Log: DateMenu.java,v $
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
 * Revision 1.7  1999/02/02 07:33:23  vecna
 * Nuked some commented out code from DateMenu and TimeZoneMenu...
 *:
 */
import java.awt.*;
import java.util.*;
import ipma.Query.*;

// assumes that typechecker has already been registered!

public class DateMenu
   extends Menu {
   public static final String CURRENT_STRING    = "Current";
   public static final String CHOOSE_STRING     = "Choose Date";
   public static final String SEPARATOR_STRING  = "-";
   public static final String DEFAULT_TITLE     = "Date";
   static final boolean       DEFAULT_TEAROFF = false;
   static final String        AT_STRING = " @ ";
   final static int  DEFAULT_NUM_DAYS  = 14;
   int      NumDays;

   public DateMenu() {
      this(DEFAULT_TITLE, DEFAULT_NUM_DAYS);
   }

   public DateMenu(String Title) {
      this(Title, DEFAULT_NUM_DAYS);
   }

   public DateMenu(int NumDays) {
      this(DEFAULT_TITLE, NumDays);
   }

   public DateMenu(String Title, int NumDays) {
      super(Title, DEFAULT_TEAROFF);
      this.NumDays = NumDays;
      BuildMenu();
   }

   protected void BuildMenu() {
      int       i;
      long      TimeLong;
      int      Year,
         Month,
         Day;
      String    TimeString;
      String    title;

      setEnabled(false);

      for (i = this.getItemCount() - 1; i >= 0; i--)
         remove(i);
      add(new MenuItem(CURRENT_STRING));

      // Get a Calendar representing the current time
      Calendar  CalLocal  = new GregorianCalendar();
      Calendar  NewCal;

      CalLocal.add(Calendar.HOUR_OF_DAY, -1);
      // Create a DateMenu that goes back numDays days.
      for (i=0; i< NumDays; i++) {
         Year  = CalLocal.get(Calendar.YEAR);
         Month = CalLocal.get(Calendar.MONTH) + 1;
         Day   = CalLocal.get(Calendar.DATE);

         CalLocal.add(Calendar.DATE, -1);
         NewCal = new GregorianCalendar();
         NewCal.setTime(CalLocal.getTime());
         add(new DateMenuItem(NewCal));
      } 
      add(new MenuItem(SEPARATOR_STRING));
      add(new MenuItem(CHOOSE_STRING));

      setEnabled(true);

      //System.out.println("DateMenu.buildMenu is called: ");
   }
}
