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
 * IRRWindow
 * 
 * IRR Window provides the Salamander connectivity, event handling, 
 * and menu building for the IRR application and applet.
 * 
 * 
 * Modification History:
 *
 * Almost complete redesign away from Salamander model
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
import java.net.*;

import ipma.Window.*;
import ipma.Help.*;

// for security classes to connect to multiple servers.
import netscape.security.PrivilegeManager;


public class IRRWindow extends CIRRWindow {
  
  public GetRequest gr;
  
 
  private final String LS_STRING = "/32,l";
  private final String MS_STRING = "/32,M";
  private final String INVALID_REQUEST_STRING = "Invalid Request";
  private final String NO_DATA_STRING = "No data returned from server";
  private final String NULL_DATA_STRING = "No data returned from server, connection possibly down.";
  private final String SECURITY_ERROR = "Security Error; Netscape Denied Priveledge(s)";
  
  private final String TOO_LARGE_STRING = "Data returned exceeds 10K";  
  private final String GEN_OPTIONS = "General Options";
  
  static protected int          DEFAULT_PORT;
  static protected String       DEFAULT_SERVER;
  private String       requestString = new String("!r");
  private Hashtable    ServersH = new Hashtable();
  private String       currentServer = new String();
  private String       result = new String();

  private MailWindow   ml_window;
  private OptionsMenu  opt_window;
  
  protected ActionListener     al = new InnerActionAdapter();
  protected ItemListener       il = new InnerItemListener();
  
   // Help menu/window stuff
   private CHelpWindowParent    Help;
   static final String HELP_TITLE   = "IRR Help";
   static final String HELP_STRING  = "Help";
   static final String HELP_TEXT[] = {
      "Right clicking  on an object brings up",
      "a mail window with the object, allowing changes",
      "to be submitted to the database\n",
      "Left clicking on a highlighted line retreives",
      "the information for that line.\n",
      "Each right click appends the current object to",
      "the editor window\n",
      "Holding CTRL and clicking button 2 marks the",
      "object for deletion in submission window"
   };
  
  private String userPwd;
  private Menu serverMenu = new Menu("Server");
   
  public IRRWindow(String server, int port) {
    
    newWindow();
    
    DEFAULT_SERVER = server;
    DEFAULT_PORT = port;
    
    // Open a connection to the server
    currentServer = server;
    
    System.out.println ("Trying to connect to " + server + ": " +  port );
    status.draw("Attempting to connect to "+server+":"+port+".");
    
    try {
      mainDisplay.setThis(this, ml_window);
      PrivilegeManager.enablePrivilege ("UniversalConnect");
      gr = new GetRequest(server,port, 0);
      
      if(gr.error==0)
	status.draw("Connected to "+gr.getSockInfo()+", awaiting request");
      else{
	System.err.println("Connection failed: "+gr.error_string + "\nConnecting to default server.");
	gr = new GetRequest(DEFAULT_SERVER, DEFAULT_PORT, 0);
	if(gr.error==0)
	  status.draw("Connected to "+gr.getSockInfo()+", awaiting request");
	else
	  status.draw("Server appears down, please try another");
      }
      
      if (gr == null);
      
      opt_window = new OptionsMenu(gr, this);
      ml_window = new MailWindow("",gr,opt_window);
     
      
      // Send the initial request with the local domain
      try{ 
	InetAddress ia = InetAddress.getLocalHost(); 
	String r = requestString+ia.getHostAddress()+LS_STRING;
	tf1.setText(ia.getHostAddress());//.substring(2).trim());
	sendRequest(r);
      }
      catch( UnknownHostException e ){
	System.out.println("Local host not known, request not sent.");
      }
      
    }
    catch (netscape.security.ForbiddenTargetException err) {
      System.out.println("Security exception: Universal Connect");
      mainDisplay.display(SECURITY_ERROR, requestString, 10000);
    } 
    
  }
  
  public final void Refresh(){
  }
   protected void newWindow(){
      
      // Call CIRRWindow.setup
      Setup();

      // add a listener for window closing events
      addWindowListener (new InnerWindowAdapter ());

      // add a mouse listener (for events for which Display sets stuff
      mainDisplay.addMouseListener (new InnerMouseAdapter ());

      // Add action listener for the submit button
      submitButton.addActionListener(al);

      // add listeners for choice 
      queryTypeChoice.addItemListener (il);
      specificC.addItemListener (il);

      // Add listeners for buttons
      buttonB.addActionListener(al);
      buttonF.addActionListener(al);

      // add a listener for returns
      tf1.addKeyListener (new KeyAdapter () {
	  public void keyPressed (KeyEvent e) {
	      int code = e.getKeyCode();
	      if (code == KeyEvent.VK_ENTER) {
		  handleSubmit ();
	      }
	  }
      });
      
      // Build the local menus
      BuildMenu();
     
      pack();
      show();
     
   }


   /* HandleSubmit: handles a submit request from the text area */
   
   private void handleSubmit(){
      
      String s = tf1.getText().trim();
      
      if(s.length()==0)
         return;
      
      try {
	PrivilegeManager.enablePrivilege ("UniversalConnect");
		 
	// If the specific list is active, we know the request is a route search
	if (specificC.isEnabled()){
	  String isSpecific = specificC.getSelectedItem();
	  
	  if(isSpecific.equals("Less Specific")){
	    if (s.indexOf("/") == -1)
	      s = s + LS_STRING;
            else if (s.indexOf(",") == -1)
	      s = s + ",l";
	  }
	  else if (isSpecific.equals("More Specific")){
            if (s.indexOf("/") == -1)
	      s = s + MS_STRING;
            else if (s.indexOf(",") == -1)
	      s = s + ",M";
	  }
	  sendRequest(requestString+s);
	  return;
	}
	
	else{
	  s = s.toUpperCase();
	  String choice_type =  queryTypeChoice.getSelectedItem();
	  if(choice_type.equals(STRING_gm)){
	    if(s.startsWith("MAINT-"))
	      sendRequest("!mmt,"+s);
	    else
	      sendRequest("!mmt,MAINT-"+s);
	  }
	  else if(choice_type.equals (STRING_ga)){
	    if(s.startsWith("AS"))
	      sendRequest("!man,"+s);
	    else
	      sendRequest("!man,AS"+s);
	  }
	  else if (choice_type.equals (STRING_gr)){
	    if(s.startsWith("AS"))
	      sendRequest("!g"+s);
	    else
	      sendRequest("!gAS"+s);
	  }
	  else if (choice_type.equals (STRING_gc))
	    sendRequest("!h"+s);
	  else if (choice_type.equals (STRING_cm)){
	    /* sendRequest(s);*/
	    String res = gr.getRawRequest(s);
	    displayRequest(res, " ", 10000);
	  }
	  else
	    sendRequest(requestString + s);
	}
      }
      catch (netscape.security.ForbiddenTargetException err) {
	System.out.println("Security Exception: Universal Connect");
	mainDisplay.display(SECURITY_ERROR, requestString, 10000);
	Close();
      }
      
   }
   private void sendRequest(String req){
      status.draw("Sending Request");
      Cursor curs = new Cursor (Cursor.WAIT_CURSOR);
      setCursor (curs);
      int bytes = gr.getNewRequest(req);
      opt_window.QtfServer.setText(gr.getSockInfo());
      opt_window.QtfPort.setText(gr.getPort());			   
      if (bytes > -1){
         status.draw("Reading "+bytes+" bytes.");
         result = gr.getBuffer();
	 if(bytes > 10000)
	   new GeneralErrorWindow("Return is very large, truncating to 10k\n", 9, 30);
         displayRequest(result,requestString, 10000);
         status.draw("Connected to: "+gr.getSockInfo()+", awaiting request");    
      }
      else {
         if(bytes == -1)
            mainDisplay.display(NO_DATA_STRING,requestString, 10000);
         if(bytes == -2)
            mainDisplay.display(INVALID_REQUEST_STRING,requestString, 10000);
         if(bytes == -3)
            mainDisplay.display(NULL_DATA_STRING,requestString, 10000);
	
      }
      curs = new Cursor (Cursor.DEFAULT_CURSOR);
      setCursor (curs);
   }
   

   public final void BuildMenu() {
      
      Menu menu;
      MenuItem menuItem;
      
      MenuBar menubar  = new MenuBar();
      
      menu = new Menu("File");
      menuItem = new MenuItem(NEW_OBJECT_STRING);
      menuItem.addActionListener(al);
      menu.add (menuItem);
      menu.addSeparator();
      menuItem = new MenuItem(CLOSE_STRING);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      menubar.add(menu);
      
      menu = new Menu("Configuration");
     
      menuItem = new MenuItem(GEN_OPTIONS);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      menubar.add(menu);

      int j = 0;
     
      Help = new CHelpWindowParent(HELP_TITLE, HELP_TEXT);
      menu     = new Menu(HELP_STRING);
      menuItem = new MenuItem (HELP_STRING);
      menuItem.addActionListener (al);
      menu.add (menuItem);
      menubar.add(menu);
      menubar.setHelpMenu(menu);
      
      setMenuBar(menubar);

   }
   

   public void Close(){
     if(ml_window != null)
       ml_window.Close();
     
     if(gr != null)
       gr.quit();
   
      setVisible (false);
     
      super.Close();
   }

    public void changeServer(String serv, int port){
     
      gr.quit();
       /**** Netscape Security stuff */
      try {
	PrivilegeManager.enablePrivilege ("UniversalConnect");
	GetRequest temp = new GetRequest(serv.trim(), port, 0);
    
	if(temp.error == 0){
	  gr = temp;
	  ml_window.changeServer(gr);
	}
	else{
	  status.draw("Please choose another server");
	}
	
      }
      catch (netscape.security.ForbiddenTargetException err) {
	mainDisplay.display(SECURITY_ERROR, requestString, 10000);
	System.out.println("Security exception: Universal Connect");
      }
     /******************************/
    }
 
   class InnerItemListener implements ItemListener  {
      public void itemStateChanged (ItemEvent e) {

         String s = ((Choice)e.getSource()).getSelectedItem();

         if (s.indexOf("Specific") < 0)
            setInfo("",s);
      }
   }

   public void setInfo(String field, String choice){
      tf1.setText(field.trim());
      queryTypeChoice.select(choice);
      infoLabel.setText((String)labelH.get(choice));
      requestString = (String)choices.get(choice);

      if (choice.equals(STRING_rs)){
         specificC.setEnabled(true);
        
      }
      else{
         specificC.setEnabled(false);
         
      }
   }

   class InnerMouseAdapter extends MouseAdapter {
      public void mousePressed (MouseEvent e) {
         String lineAt=mainDisplay.getMouseDown();
         
         if(lineAt == null) return;

         // We don't want to do a route search on a route search. (At least not yet)
         if(lineAt.startsWith("route")) return;
         
         if(lineAt.startsWith("mnt-by")){
            lineAt=lineAt.substring(lineAt.lastIndexOf(" ")+1);
            setInfo(lineAt,STRING_gm);
            sendRequest("!mmt,"+lineAt);
            return;
         }
         if(lineAt.startsWith("origin")){
            lineAt=lineAt.substring(lineAt.lastIndexOf(" ")+1);
            setInfo(lineAt,STRING_ga);
            sendRequest("!man,"+lineAt);
            return;
         }
         if(lineAt.indexOf('/') > 0 
	    && lineAt.indexOf('.') > 1 
	    && lineAt.indexOf('.') < 4){
            setInfo(lineAt, STRING_rs);
            sendRequest("!r"+lineAt+",l");
            return;
         }
      }
   }

   class InnerWindowAdapter extends WindowAdapter {
       public void windowClosing (WindowEvent e) {
	  
	   Close();
	   System.exit(1);
	}
   }
    

   class InnerActionAdapter implements ActionListener {
      public void actionPerformed (ActionEvent e) {
         String s = e.getActionCommand ();

         if(s.equals("B")){
            mainDisplay.back();
            return;
         }
         
         if(s.equals("F")){
            mainDisplay.forward();
            return;
         }

         if (s.equals ("Submit")) {
            handleSubmit();
            return;
         }

         if (s.equals (CLOSE_STRING)) {
            Close();
	    System.exit(1);
 	    return;
         }
	
         if(s.equals(HELP_STRING)){
            Help.ShowHelpWindow();
            return;
         }
         
         if(s.equals(NEW_OBJECT_STRING)){
            new MailWindow(gr, opt_window);
            return;
         }
	
	 if(s.equals(GEN_OPTIONS)){
	 
	   opt_window.showAgain();
	     }
      }
   }   

} // End of Class Definiton

   
