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
 * GeneralDialog
 *
 * Upper Atmospheric Research Collaboratory
 * School of Information
 * University of Michigan at Ann Arbor
 * 610 East University, Ann Arbor, MI 48109
 *
 *
 * Copyright (c) 1998 The Regents of The University of Michigan.
 * All rights reserved.
 *
 * Contact: jbres@umich.edu
 *
 * 
 * A dialog with a message and a given string named button.
 * 
 * Modification History:
 * @date 5/18/1998
 * @version 1.1 
 * @author Sushila Subramanian
 * Modified package name to work in IPMA project. Also extended
 * it to take any string on the button, and to return the button
 * so another object can "listen" to it if necessary.
 *
 * @version @#@
 * @date 9/5/97
 * @author J. Breslau
 */

package ipma.Util;

import java.awt.*;
import java.awt.event.*;

public class GeneralDialog extends Dialog {
   
   private Button darn;
   private Button[] buttons; // if multiple buttons at bottom.

   /** GeneralDialog : Constructor.
    *  @param f Frame to which dialog is attached.
    *  @param s Name for dialog box.
    */

   public GeneralDialog(Frame f, String s) {
      super(f, s, true);
      setLayout(new BorderLayout());
      Label l = new Label(s, Label.CENTER);
      add(l, BorderLayout.NORTH);
      darn = new Button("Darn");
      add(darn, BorderLayout.SOUTH);
      darn.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent e) {
         setVisible(false);
         }
         });
      addWindowListener(new WindowAdapter() {
         public void windowClosing(WindowEvent e) {
         setVisible(false);
         }
         });
      pack();
      setVisible(true);
   }

   /** GeneralDialog: Constructor.
    *  @param f Frame to which dialog is attached.
    *  @param s Name for dialog box.
    *  @param buttonName Name for button at the bottom.
    */

   public GeneralDialog(Frame f, String s, String buttonName) {
      super(f, s, true);
      setLayout(new BorderLayout());
      Label l = new Label(s, Label.CENTER);
      add(l, BorderLayout.NORTH);
      darn = new Button(buttonName);
      add(darn, BorderLayout.SOUTH);
      darn.addActionListener(new ActionListener() {
         public void actionPerformed(ActionEvent e) {
         setVisible(false);
         }
         });
      addWindowListener(new WindowAdapter() {
         public void windowClosing(WindowEvent e) {
         setVisible(false);
         }
         });
      pack();
      setVisible(true);
   }

   /** GeneralDialog: Constructor.
    *  @param f Frame to which dialog is attached.
    *  @param s Name for dialog box.
    *  @param buttonNames  Array of strings for multiple buttons at bottom
    */

   public GeneralDialog(Frame f, String s, String[] buttonNames,
      ActionListener aListener) {
      super(f, s, true);

      Panel buttonpanel = new Panel ();
      buttonpanel.setLayout (new GridLayout (1, 0));

      setLayout(new BorderLayout());
      Label l = new Label(s, Label.CENTER);
      add(l, BorderLayout.NORTH);
      ActionListener al = new ActionListener() {
         public void actionPerformed(ActionEvent e) {
         setVisible(false);
         }
         };

      buttons = new Button [buttonNames.length];

      
      for (int i=0; i <buttonNames.length; i++) {
         buttons[i] = new Button (buttonNames[i]);
         buttonpanel.add (buttons[i]);
         // add input listener if any, and the one here as well.
         if (aListener != null) 
            buttons[i].addActionListener (aListener);
         buttons[i].addActionListener (al);
      }

      add(buttonpanel, BorderLayout.SOUTH);
      addWindowListener(new WindowAdapter() {
         public void windowClosing(WindowEvent e) {
         setVisible(false);
         }
         });

      pack();
      //setSize (200,50);
      setVisible(true);
   }

   // access method for button in case some other object also
   // wants to "listen" for it and take action based on it.
   public Button getButton () {
      return darn;
   }

   // access method for array of buttons in case some other object also
   // wants to "listen" for it and take action based on it.
   public Button[] getButtons () {
      return buttons;
   }
}
