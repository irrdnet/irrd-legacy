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
 * CWindow
 * 
 * CWindow extends the basic Frame class by adding the ability to hide
 * itself, etc.
 * 
 * Modification History:
 * Original code was written in java 1.0.2. This version is a port
 * to jdk 1.1
 *
 * @version 1.1
 * @date 23 April 1998
 * @author Jimmy Wan, port to 1.1 by Sushila Subramanian
 */



import java.awt.*;
import java.awt.event.*;

public class CWindow
extends Frame {
  
  private static final Color FCOLOR = Color.black;
  private static final Color BCOLOR = Color.lightGray;

  /** CWindow: Constructor.
   */

  public CWindow() {
    super();

	 SetupFont();

	 setForeground(FCOLOR);
    setBackground(BCOLOR);
    addWindowListener (new InnerWindowAdapter ());
  }

  /** CWindow: Constructor with some initialized variables.
   *  @param TitleString Title for window.
   *  @param c Component to be added.
   */
  
  public CWindow(String TitleString, Component c) {
    super();
	 SetupFont();
    setForeground(FCOLOR);
    setBackground(BCOLOR);
    addWindowListener (new InnerWindowAdapter ());
    Setup(TitleString, c);
  }
  
  
  /** CWindow: Constructor with some all variables initialized.
   *  @param TitleString Title for window.
   *  @param c Component to be added.
   *  @param width Width of window.
   *  @param height Height of window.
   */

  public CWindow(String TitleString, Component c,
		 int width, int height) {
    
    super();
	 SetupFont();
    setForeground(FCOLOR);
    setBackground(BCOLOR);
    addWindowListener (new InnerWindowAdapter ());
    Setup(TitleString, c, width, height);
  }
  
  public void SetupFont(){
	 /*
		Font f;
	 f = new Font("Courier", Font.BOLD | Font.ITALIC, 12);
	 if (f != null){
		setFont(f);
		System.out.println("A CWindow is constructued");
	 }
	 */
  }

  /** Setup: Build layout for window with components and 
   *  set title.
   *  @param TitleString Title for window.
   *  @param c Component to be added to window.
   */
  
  protected void Setup(String TitleString, Component c) {
     Setup(TitleString, c, 320, 200);
  }
  
  /** Setup: Build layout for window with components and 
   *  set title.
   *  @param TitleString Title for window.
   *  @param c Component to be added to window.
   *  @param width Width for window.
   *  @param height Height for window.
   */

  protected void Setup(String TitleString, Component c,
		       int width, int height) {
    BuildLayout(c);
    this.setTitle(TitleString);

    setBounds(0, 0, width, height);
  }

  public void setVisible(boolean b) {
    Insets  insets;
    Point   location;

    super.setVisible(b);
    insets   = getInsets();
    location = getLocation();

    setLocation(location.x + insets.left, location.y + insets.top);
    setLocation(insets.left, insets.top);
  }
  
  /** BuildLayout: Builds a simple grid layout, and adds
   *  the given component.
   *  @param c Component to be added.
   */
  
  private final void BuildLayout(Component c) {
    
    setLayout(new GridLayout(1, 1));
    add(c);
  }

  /* i'm assuming this does the same thing as catching a 
   * Window_Destroy - should check on this.
   */

  class InnerWindowAdapter extends WindowAdapter {
    public void windowClosing(WindowEvent e) {
      Close();
    }
  }

  synchronized public void Close() {
    setVisible (false);
  }
}
