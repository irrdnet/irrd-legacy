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
 * Display
 * 
 * Main display canvas.
 * 
 * 
 * Modification History:
 *
 * Slight modifications to do
 * TCP autoappend deletion
 * @5/13/99 
 * @author Jon Poland
 * 
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
import java.awt.event.*;
import java.util.*;

public class Display extends Canvas{
  
  String s = new String();
  Vector v = new Vector();
  Font f = new Font("Courier", Font.PLAIN, 12);
  IRRWindow irrw;
  MailWindow mailw;
    
  //Vector and current index for back and forward
  Vector savedStates = new Vector();
  int currentState = -1;
  boolean newData = true;

  Scrollbar scroll_up = new Scrollbar(Scrollbar.VERTICAL);
  
  // Beginning x and y
  int X=0;
  int Y=0;
  
  int lineHeight;
  int startLine = 0;
  int displayLines = 25;   // Just a default
  int totalLines;
  String lastLineAt;
  
  Dimension dim;
  FontMetrics fontM;
  
  boolean hasData = false;
  boolean newRequest = true;
  boolean routes  = false;
  
  int sources[];
  
  private String DEL_STRING = "delete: TCP IRRj Deletion\n";
  /* 
     End of variable declarations
     Beginning of real code

     */


  public Display(){
    setFont(f);
    fontM = getFontMetrics(getFont());
    lineHeight = fontM.getHeight()+1;

    // add a listener for scrollbar events

    scroll_up.addAdjustmentListener (new AdjustmentListener () {
      public void adjustmentValueChanged (AdjustmentEvent e) {
	scrollup (scroll_up.getValue());
      }
    });

    // add a listener for mouse events. 
    addMouseListener (new InnerMouseAdapter ());
  }
  
  public void paint(Graphics g){
    newRequest = false;
    update(g);
  }
  
  public void update(Graphics g) {
    int x=X;
    int y=Y;
    int count;
    String showString = new String();

    dim = getSize();
    displayLines = (int)dim.height/(int)lineHeight -1; // Keep the last line whole
    setBackground(Color.white);
    g.clearRect(X,Y,dim.width,dim.height);
    
    if(totalLines<=displayLines){
      count = totalLines;
      startLine = 0;
      scroll_up.setEnabled (false);
    }
    else{
      count=displayLines;
      scroll_up.setEnabled (true);
    }
    
    if(newRequest)
      scroll_up.setValue(scroll_up.getMinimum());
    
    if(totalLines<count+startLine)
      startLine = totalLines-count;
    
    for(int i=startLine;i<count+startLine;i++){
      
      // Get the next line to display
      showString = ((String)v.elementAt(i));
      if(routes || showString.startsWith("mnt-by") || showString.startsWith("origin")){
	g.setColor(Color.blue);
	g.drawString(showString,x,y+=lineHeight);
	g.setColor(Color.black);
      }
      else
	  g.drawString(showString,x,y+=lineHeight);
    }
  }  
  
  public void display(String str, String prefix, int num){
    v = new Vector();
    s = str;
    int index=1;
    int tokens=0;
    String newElement;
    
    hasData = true;
    
    StringTokenizer st = new StringTokenizer(s,"\n",false);
    totalLines = st.countTokens();
    sources = new int[totalLines+1];
    sources[1] = 0;

    tokens = totalLines;

    while(tokens-- > 0){
      newElement = st.nextToken();//+"\r";
      v.addElement(newElement);
      if(newElement.startsWith("source")){
	v.addElement("");
	sources[++index] = totalLines-tokens;
	totalLines++;
      }
    }
    sources[0]=index;
    
    routes = false;
    saveState(v,prefix, num);
    repaint();
  }
  
  public void displayRoutes(String str, String prefix, int numRoutes){
    v = new Vector();
    s = str;
    int i = 0;

    hasData = true;
    StringTokenizer st = new StringTokenizer(s);
    while(st.hasMoreTokens() & i < numRoutes){
      v.addElement(st.nextToken());
      i++;
    }
    totalLines = v.size();
    
    routes = true;
    saveState(v,prefix, numRoutes);
    repaint();
  }
    
  public void saveState(Vector vec,String prefix, int num){
    newRequest = true;
    startLine=0;

    if(newData){ 
      // Save this request
      if(savedStates.size() != currentState+1)
	savedStates.setSize(currentState+1);
      
      savedStates.addElement(new SaveState(routes,vec,irrw.tf1.getText(),prefix, num));
      
      currentState++;
    }
    newData = true;
  }
    
  public void scrollup(int value){
    int i1;
    
    double d1 = (double)value/90d;
    int d2 = totalLines-displayLines;
    i1 = (int)Math.round(d1*d2);
    
    if(startLine != i1){
      startLine = i1;
      newRequest = false;
      repaint();
    }
  }
  
  public int getLineNumber(int y){
    //y-=4;
    if(y>dim.height || y<0 || displayLines==0 || !hasData)
      return -1;

    int i = (int)((float)y/(float)lineHeight) + startLine;

    return(i);
  }
  
  public String getLineAt(int y){
    int i = getLineNumber(y);
    if(i>=totalLines || i<0)
      return null;
    return (String)v.elementAt(i);
  }
  
  public String getObjectAt(int y){
    int route=0;
    int start=0;
    String str = new String();
    
    if (sources[2] == 0)
	return(s.trim());
   
    
    int lineNumber = getLineNumber(y);
    
    for (int i=1;i<sources[0];i++){
	if (lineNumber>sources[i])
	    route = i;
    }
    
    if(route>1)
	start = sources[route]+1;

    for(int i=start;i<=sources[route+1];i++)
      str += (String)v.elementAt(i) + '\n';
    
    return(str.trim());
  }

    public String getCurrObj(){
	String str = new String();
	for(int i = 0; i<(totalLines-1);i++)
	    str += (String)v.elementAt(i) + '\n';
	return(str.trim());
    }
    
    public void setThis(IRRWindow irrw, MailWindow mail_win){
	this.irrw = irrw;
	this.mailw = mail_win;
    }
    
    class InnerMouseAdapter extends MouseAdapter {
    public void mousePressed (MouseEvent e) {
      String lineAt;
      String token,beg;

      /* If there is a right click on an object, present that object in a new 
	 MailWindow, after changing things like 'route' back to '*rt' 
	 */
      try{
	  if ((e.getModifiers()==e.BUTTON2_MASK ||
               e.getModifiers()==e.BUTTON3_MASK) && hasData && !routes) {
	      if ((lineAt = getObjectAt(e.getY())) != null){
		  
		  StringTokenizer st = new StringTokenizer(lineAt,"\n");
		  lineAt = new String();
		  
		  while(st.hasMoreTokens()){
		      token = st.nextToken();
		      lineAt = lineAt+token+"\n";
		  }
		  mailw.appendText(lineAt+"\n");
		  mailw.show();
		  
	      }
	      return;
	  }
	  else if(e.getModifiers() == (e.BUTTON2_MASK + e.CTRL_MASK) &&
                  hasData && !routes){
	      if ((lineAt = getObjectAt(e.getY())) != null){
		  
		  StringTokenizer st = new StringTokenizer(lineAt,"\n");
		  lineAt = new String();
		  
		  while(st.hasMoreTokens()){
		      token = st.nextToken();
		      lineAt = lineAt+token+"\n";
		  }
		  mailw.appendText(lineAt);
		  mailw.appendText(DEL_STRING + "\n");
		  mailw.show();
	      }
	      return;
	  }
	  lastLineAt = getLineAt(e.getY());
      }
      catch(ArrayIndexOutOfBoundsException noObject){
	  System.out.print("No Object\n");
      }
    }    
    }
    
    public String getMouseDown(){
	return lastLineAt;
    }
    
  public void back(){
    if(currentState < 1)
      return;

    SaveState t = (SaveState)savedStates.elementAt(--currentState);
    
    newData = false;

    irrw.setInfo(t.showInField,t.prefix);

    if(t.routes)
      displayRoutes(t.result,t.prefix, t.max_num);
    else
      display(t.result,t.prefix, t.max_num);
  }
  
  public void forward(){
    if(currentState+1 >= savedStates.size())
      return;

    SaveState t = (SaveState)savedStates.elementAt(++currentState);

    newData = false;

    irrw.setInfo(t.showInField,t.prefix);

    if(t.routes)
      displayRoutes(t.result,t.prefix,t.max_num);
    else
      display(t.result,t.prefix, t.max_num);
  }

  
} // End of class definition

class SaveState{

  String result = new String();
  boolean routes;
  String showInField;
  String prefix;
  int max_num;

  public SaveState(boolean routes,Vector display,String str,String prefix, int num){
    this.routes  = routes;
    this.showInField = str;
    this.prefix = prefix;
    this.max_num = num;

    Enumeration e = display.elements();
    
    while(e.hasMoreElements())
      result = result + e.nextElement() + '\n';
  }
}
  
