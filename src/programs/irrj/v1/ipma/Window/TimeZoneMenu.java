package ipma.Window;

import java.awt.*;
import java.util.*;

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
 * TimeZoneMenu
 *
 * 
 * Modification History:
 * $Log: TimeZoneMenu.java,v $
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
 * Revision 1.3  1999/02/03 22:43:54  vecna
 * Added more TimeZones.  Use if pretty self-explanatory...
 *
 * Revision 1.2  1999/02/02 07:33:24  vecna
 * Nuked some commented out code from DateMenu and TimeZoneMenu...
 *:
 *
 */

public class TimeZoneMenu
   extends Menu{

   final static String  DEFAULT_TITLE = "Time Zone";
   final static boolean DEFAULT_TEAROFF = false;
   final static String  BEFORE_GMT    = "GMT+";
   final static String  AFTER_GMT     = "GMT-";
   protected Menu  BeforeGMT,
                   AfterGMT;
   protected MenuItem DefaultItem,
                      UTCItem;

   public TimeZoneMenu() {
      this(DEFAULT_TITLE);
   }
   
   public TimeZoneMenu(String Title) {
      super(Title, DEFAULT_TEAROFF);
      BuildMenu();
   }

   protected void BuildMenu() {
      TimeZoneMenuItem  Item;
      String   AllItems[];
      TimeZone TZ;
      int   i,
            j;
      DefaultItem = new TimeZoneMenuItem(TimeZone.getDefault());
      add(DefaultItem);
      UTCItem = new TimeZoneMenuItem(SimpleTimeZone.getTimeZone("UTC"));
      add(UTCItem);

      BeforeGMT = new Menu(BEFORE_GMT);
      AfterGMT  = new Menu(AFTER_GMT);
      AllItems = TimeZone.getAvailableIDs();
      i = 0;
      j = AllItems.length;
      while (i < j) {
         TZ = SimpleTimeZone.getTimeZone(AllItems[i]);
         Item = new TimeZoneMenuItem(TZ);
         if (TZ.getRawOffset() >= 0)
            BeforeGMT.add(Item);
         else
            AfterGMT.add(Item);
         i++;
      }
      add(BeforeGMT);
      add(AfterGMT);
   }

   public MenuItem[] getItems() {
      int   BeforeCount,
            AfterCount;
      int   i,
            j,
            Index;
      MenuItem Items[];
      MenuItem Item;

      BeforeCount = BeforeGMT.getItemCount();
      AfterCount = AfterGMT.getItemCount();
      i = 0;
      j = 2 + BeforeCount + AfterCount;
      Items = new MenuItem[j];

      Items[i++] = DefaultItem;
      Items[i++] = UTCItem;
      Index = 0;
      while (Index < BeforeCount) {
         Items[i++] = BeforeGMT.getItem(Index++);
      }
      Index = 0;
      while (Index < AfterCount) {
         Items[i++] = AfterGMT.getItem(Index++);
      }
      return Items;
   }
}
