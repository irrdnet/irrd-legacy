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
 * CPopupWindow
 * 
 * CPopupWindow extends CWindow to add a close button.  This was done
 * since this will likely appear multiple times.
 */


/**
 * Modification History:
 * $Log: CPopupWindow.java,v $
 * Revision 1.1  2000/08/19 14:35:56  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:10  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:15  polandj
 * Adding irrj under repository
 *
 * Revision 1.5  1999/03/09 21:53:30  vecna
 * Changed all instances of "show()" to "setVisible(true)".
 * Modified default size to be 320*200.  Fixed annoying problem of windows
 * popping up with title bar off the screen...
 *
 * Revision 1.4  1999/02/02 07:33:22  vecna
 * Nuked some commented out code from DateMenu and TimeZoneMenu...
 *:
 */
import java.awt.*;
import java.awt.event.*;

public class CPopupWindow
   extends CWindow {
   private static final String CLOSE_STRING = "Close";
   private final Panel    MainPanel   = new Panel();
   
   private final Button   CloseButton = new Button(CLOSE_STRING);
   /** CPopupWindow: Constructor.
    */
   
   public CPopupWindow() {
      setForeground(Color.black);
      setBackground(Color.lightGray);
   }

   /** CPopupWindow: Constructor with some initialized variables.
    *  @param TitleString Title for window.
    *  @param c Component to be added to window.
    */

   public CPopupWindow(String TitleString, Component c) {
      setForeground(Color.black);
      setBackground(Color.lightGray);
      Setup(TitleString, c);
   }
   
   /** CPopupWindow: Constructor with all variables initialized.
    *  @param TitleString Title for window.
    *  @param c Component to be added.
    *  @param width Width of window.
    *  @param height Height for window.
    */

   public CPopupWindow(String TitleString, Component c,
      int width, int height) {
      setForeground(Color.black);
      setBackground(Color.lightGray);
      Setup(TitleString, c, width, height);
   }
   
   /** Setup Sets up the layout for components.
    *  @param TitleString Title for window.
    *  @param c Component to be added.
    */

   protected void Setup(String TitleString, Component c) {
      BuildLayout(c);
      super.Setup(TitleString, MainPanel, 320, 200);
   }

   /** Setup Sets up the layout for components.
    *  @param TitleString Title for window.
    *  @param c Component to be added.
    *  @param width Width for window.
    *  @param height Height for window.
    */

   protected void Setup(String TitleString, Component c,
      int width, int height) {

      BuildLayout(c);
      super.Setup(TitleString, MainPanel, width, height);
   }

   /** BuildLayout: Creates a layout for the window with
    *  the input components included.
    *  @param c Component to be added.
    */

   private void BuildLayout(Component c) {

      GridBagLayout        g;
      GridBagConstraints   gc;
      
      CloseButton.setForeground(Color.black);
      CloseButton.setBackground(Color.lightGray);
      // add inner class that will handle button actions.

      // an actionListener for Buttons.
      CloseButton.addActionListener (new ActionListener() {
         public void actionPerformed(ActionEvent e) {
         String label = e.getActionCommand();


         if (label.equals (CLOSE_STRING)) {
         Close ();
         }
         }
         });
      
      g  = new GridBagLayout();
      gc = new GridBagConstraints();
      
      MainPanel.setLayout(g);


      
      gc.fill       = GridBagConstraints.BOTH;
      gc.gridwidth  = GridBagConstraints.REMAINDER;
      gc.gridheight = GridBagConstraints.RELATIVE;
      gc.weightx    = 10.0;
      gc.weighty    = 10.0;
      g.setConstraints(c, gc);
      MainPanel.add(c);
      
      gc.anchor     = GridBagConstraints.CENTER;
      gc.fill       = GridBagConstraints.NONE;
      gc.gridheight = GridBagConstraints.REMAINDER;
      gc.weightx    = 0.0;
      gc.weighty    = 0.0;
      g.setConstraints(CloseButton, gc);
      MainPanel.add(CloseButton);
   }
   
   /** setForeground : Sets foreground colour.
    *  @param c Color.
    */

   public void  setForeground(Color c) {
      if (MainPanel != null)
         MainPanel.setForeground(c);
      super.setForeground(c);
   }
   
   /** setBackground: Sets the background colour.
    *  @param c Colour for background.
    */

   public void  setBackground(Color c) {
      if (MainPanel != null)
         MainPanel.setBackground(c);
      super.setBackground(c);
   }
}
