package irr;

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
 * CIRRWindow
 * 
 * CIRRWindow provides the base window class for the IRR 
 * applet/applications it handles setup, data and request 
 * handling, and layout management
 * 
 * Modification History:
 * Port to jdk1.1.
 * @version 1.1.
 * @date 11 June 1998.
 * @author Sushila Subramanian.
 *
 * @version 1.0
 * @date 
 * @author Aaron Klink.
 */


import java.awt.*;
import java.util.*;
import java.lang.*;
import ipma.Window.*;

public class CIRRWindow extends CPopupWindow {
  TextField tf1;
  
  static final int FRAME_WIDTH  = 640;
  static final int FRAME_HEIGHT = 800;
  
  static final Font FONT = new Font("Courier", Font.PLAIN, 12);
  
  static final String WINDOW_TITLE = "IRR";
  static final String CLOSE_STRING = "Close";
  static final String NEW_OBJECT_STRING = "Create New Object";
  
  static final String STRING_rs = new String("Route Searches");
  static final String STRING_ga = new String("Get aut-num object");
  static final String STRING_gr = new String("Get Route with Origin");
  static final String STRING_gm = new String("Get Maintainer");
  static final String STRING_gc = new String("Get Routes with Community");
  static final String STRING_cm = new String("Send Command");

  static final String ISEP_CHAR = new String(": ");
  
  Hashtable choices = new Hashtable();
  Hashtable choicesReverse = new Hashtable();
  Hashtable labelH = new Hashtable();
  
  Choice specificC = new Choice();
  Choice queryTypeChoice = new Choice();
  
  Button submitButton; 

  // infoLabel changes when the user selects a new query
  Label  infoLabel = new Label();

  // status provides the server status messages
  Status status = new Status();
  
  // Forward and back buttons
  Button buttonB = new Button("B");
  Button buttonF = new Button("F");

  // 
  Display mainDisplay = new Display();
  
  /** Setup: Initial setup of window, including choices, 
   *  buttons etc.
   */
  
  public void Setup(){
    
    super.setTitle(WINDOW_TITLE);
    
    // Set up choice for specifics
    specificC.addItem("Less Specific");
    specificC.addItem("Specific");
    specificC.addItem("More Specific");

    // Set up the hash for the labels
    labelH.put(STRING_rs,"Specify a route number, such as 141.212.10.2/24");
    labelH.put(STRING_ga,"Specify an AS number, such as 1234, or AS1234");
    labelH.put(STRING_gr,"Specify an AS number, such as 1234, or AS1234");
    labelH.put(STRING_gm,"Specify a maintainer such as AS1800");
    labelH.put(STRING_gc,"Specify a community, such as COMM_NSFNET");
    labelH.put(STRING_cm,"Send an arbitrary command, such as !gAS1234");
    
    // Set up the hash for the choices
    choices.put(STRING_cm," ");
    choices.put(STRING_rs,"!r");
    choices.put(STRING_ga,"!man,AS");
    choices.put(STRING_gr,"!gAS");
    choices.put(STRING_gm,"!mmt,MAINT-AS");
    choices.put(STRING_gc,"!h");
    
    Enumeration e = choices.keys();
    String str = new String();
    while(e.hasMoreElements()){
      str = (String)e.nextElement();
      choicesReverse.put(choices.get(str),str);
    }

    // Make the info label info come up red
    infoLabel.setForeground(Color.red);
    
    // Setup the layout
    newLayout();
  }
  
  public void newLayout(){
    Panel p;
    
    setLayout(new BorderLayout());
    
    // Setup canvas
    p = new Panel();
      p.setLayout(new BorderLayout());
      mainDisplay.setSize(400,400);
      p.add("Center",mainDisplay);
      p.add("East",mainDisplay.scroll_up);
    add("Center",p);
    
    
    p = new Panel();
      Enumeration e = choices.keys() ;
      String s = (String)e.nextElement();
      queryTypeChoice.addItem(s);
      infoLabel.setText((String)labelH.get(s));
      
      while(e.hasMoreElements())
	queryTypeChoice.addItem((String)e.nextElement());
    
      Panel p1 = new Panel();
        p1.add(queryTypeChoice);
    	tf1 = new TextField(20);
	p1.add(tf1);
        submitButton = new Button("Submit");
	p1.add(submitButton);
      Panel p2 = new Panel();
        p2.setLayout(new FlowLayout());
	p2.add(buttonB);
	p2.add(buttonF);
        p2.add(specificC);
      p.setLayout(new BorderLayout());
      p.add("North",p1);
      p.add("Center",p2);
      infoLabel.setAlignment(Label.CENTER);
      p.add("South",infoLabel);
    add("North",p);
    
    p = new Panel();
      status.setSize(475,20);
      p.add(status);
    add("South",p);
    queryTypeChoice.select(STRING_rs);
    infoLabel.setText((String)labelH.get(STRING_rs));
    specificC.setEnabled(true);
  }

  /** displayRequest : depending on what line we click at,
   *  display appropriate result.
   *  @param b String to switch on.
   */
  
  public void displayRequest(String b,String prefix, int num){
   
    String result = new String();
    String str,type;
    char c = b.charAt(0);
    prefix = (String)choicesReverse.get(prefix);
    /* apparently a Character and a char are not the same..cannot use isDigit()..grr! */
    if ( c == '1' || c == '2' || c == '3' || c == '4' || c == '5' ||
	 c == '6' || c == '7' || c == '8' || c == '9' || c == ' '){
      result = b.replace(' ','\n');
     
      mainDisplay.displayRoutes(result,prefix, num);
      return;
    }

    mainDisplay.display(b,prefix, num);
   
    return;
  }
  
} // End of Class Definition

class Status extends Canvas{
  String string = "";

  public void paint(Graphics g) {
    update(g);
  }
     
  public void update(Graphics g) {
    int w = getSize().width;
    int h = getSize().height;
    
    g.clearRect(0, 0, w, h);
    
    g.drawString(string,0,15);
  }

  public void draw(String str){
    this.string = str;
    repaint();
  }
}

// End of CIRRWindow declaration

