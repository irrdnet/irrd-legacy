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

/* Options Menu
 *
 * Basic per session options such as Query server, submit server, password, 
 * databases.
 *
 * 5/3/99
 * Jon Poland
 */

import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.net.*;
import java.io.*;
import ipma.Window.*;
import netscape.security.PrivilegeManager;

public class OptionsMenu extends CWindow{
  
  GetRequest gr;   /* passed in during creation, used to change servers */
  IRRWindow irrw; /* the parent IRRWindow */
  private Font f = new Font("Courier", Font.PLAIN, 12);
  
    Label      DBLabel = new Label("Query Databases");
    Label      QportLabel = new Label("Query Port  ");
    Label      QserverLabel = new Label("Query Server  ");
    Label      SserverLabel = new Label("Submit Server");
    Label      SportLabel = new Label("Submit port");
    Label      pwdLabel = new Label("Mntner Password");
    Label      pgpPwdLabel = new Label("PGP Password     ");
    Label      pgpUidLabel = new Label("PGP User ID        ");
    Label      pgpExecLabel = new Label("PGP executable   ");
    Label      pgpPathLabel = new Label("Absolute path to PGP rings");
    redLabel   statLabel = new redLabel();
    Label      availableLabel = new Label("Available Databases");
    Label      selectedLabel = new Label("Selected DB's(search top to bottom)");
  
  TextField  QtfServer = new TextField(20);
  TextField  QtfPort   = new TextField(5);
  TextField  StfServer = new TextField(20);
  TextField  StfPort   = new TextField(5);
  TextField  Passwd    = new TextField(15);
  TextField  pgpPwd = new TextField(15);
  TextField  pgpUid  = new TextField(15);
  TextField  pgpExec = new TextField(15);
  TextField  pgpPath = new TextField(25);
  // Connection stuff
  private InetAddress ia;
  
  // Listener stuff
  protected ActionListener al = new InnerActionListener();
  
  // String constants
  private String SUBMIT_STRING = "Ok";
  private String CLOSE_STRING  = "Cancel";
  private String DEFAULT_STRING = "Load Defaults";
  static final String WINDOW_TITLE = "General Options";
  
  //private final String DEFAULT_SERVER = "radb1.merit.edu";
  // private final String DEFAULT_QPORT = "43";
  private final String DEFAULT_SPORT = "8888";
  private final String DB_REQUEST = "!s-lc";
  private final String DB_RESET = "!s-*";
  private final String DB_SELECT = "!s";

    //Strings for saving state, for cancel button
    private String old_qserver;
    private String old_sserver;
    private String old_qport;
    private String old_sport;
    private String old_pwd;
    private String[] old_available;
    private String[] old_selected;

  /*protected ItemListener il = new InnerItemListener ();;*/
  
  /* New */
/*  TextArea db_available = new TextArea("", 8, 20,
TextArea.SCROLLBARS_VERTICAL_ONLY);
  TextArea db_selected = new TextArea("",8, 20,
TextArea.SCROLLBARS_VERTICAL_ONLY);*/

  List db_available = new List(7, true);
  List db_selected = new List(7, true);  


  public OptionsMenu(GetRequest g_r, IRRWindow irr_win){
    super.setTitle(WINDOW_TITLE);
    gr = g_r;
    irrw = irr_win;
   
    Passwd.setEchoChar('*');
    pgpPwd.setEchoChar('*');

    int bytes = gr.getNewRequest(DB_REQUEST);
    if (bytes < 1) return;
    String s = gr.getBuffer();
    if (s == null) return;
    BuildLayout(s.trim());
    addWindowListener(new InnerWindowAdapter ());
    old_qserver = QtfServer.getText();
    old_sserver = StfServer.getText();
    old_qport = QtfPort.getText();
    old_sport = StfPort.getText();
    old_pwd = Passwd.getText();
//    old_available = db_available.getText();
  //  old_selected = db_selected.getText();
    old_available = db_available.getItems();
    old_selected = db_selected.getItems();
   // db_available.setEditable(false);
   // db_selected.setEditable(false);
    pack();
    show();
  }
  
  protected void BuildLayout(String s){

    
    Panel p = new Panel();
    GridBagLayout  g = new GridBagLayout();
    p.setLayout(g);
    GridBagConstraints gbc = new GridBagConstraints();
    gbc.gridwidth = GridBagConstraints.REMAINDER;
    gbc.anchor = GridBagConstraints.WEST;

    String str;
    int index=0;
    
   
    Panel db_p = new Panel();
    //db_selected.setText(s.replace(',', '\n'));
    StringTokenizer strTok = new StringTokenizer(s, ",", false);
    while(strTok.hasMoreTokens())
       db_selected.addItem(strTok.nextToken());    

    Panel left_p = new Panel();
    left_p.setLayout(new BorderLayout());
    left_p.add(availableLabel, "North");
    left_p.add(db_available, "South");
    db_p.add(left_p);
    
    Panel mid_button = new Panel();
    mid_button.setLayout(new BorderLayout());
    Button tmpButton = new Button("-->");
    mid_button.add(tmpButton, "North");
    tmpButton.addActionListener(al);
    tmpButton = new Button("<--");
    mid_button.add(tmpButton, "South");
    tmpButton.addActionListener(al);
    db_p.add(mid_button);
    
    Panel right_p = new Panel();
    right_p.setLayout(new BorderLayout());
    right_p.add(selectedLabel, "North");
    right_p.add(db_selected, "South");
    db_p.add(right_p);
    
    add(db_p, "North");

    Panel serverP = new Panel();
    serverP.add(QserverLabel);
    QtfServer.setText(gr.getSockInfo());
    serverP.add(QtfServer);
    QtfPort.setText(gr.getPort());
    serverP.add(QportLabel);
    serverP.add(QtfPort);
    
    g.setConstraints(serverP, gbc);
    p.add(serverP);
  
    
    Panel S = new Panel();
    S.add(SserverLabel);
    StfServer.setText(gr.getSockInfo());
    StfPort.setText(DEFAULT_SPORT); 
    S.add(StfServer);
    S.add(SportLabel);
    S.add(StfPort);
    g.setConstraints(S, gbc);
    p.add(S);
 
    
    Panel Pass = new Panel();
    Pass.add(pwdLabel);
    Pass.add(Passwd);
    g.setConstraints(Pass, gbc);
    p.add(Pass);
     Pass = new Panel();
    Pass.add(pgpPwdLabel);
    Pass.add(pgpPwd);
    g.setConstraints(Pass, gbc);
    p.add(Pass);
     Pass = new Panel();
    Pass.add(pgpUidLabel);
    Pass.add(pgpUid);
    g.setConstraints(Pass, gbc);
    p.add(Pass);
     Pass = new Panel();
    Pass.add(pgpExecLabel);
    Pass.add(pgpExec);
    g.setConstraints(Pass, gbc);
    p.add(Pass);
     Pass = new Panel();
    Pass.add(pgpPathLabel);
    Pass.add(pgpPath);
    g.setConstraints(Pass, gbc);
    p.add(Pass);
    p.add(statLabel);
    add(p);
    
    p = new Panel();
    tmpButton = new Button(SUBMIT_STRING);
    tmpButton.addActionListener(al);
    p.add(tmpButton);
    tmpButton = new Button(CLOSE_STRING);
    tmpButton.addActionListener(al);
    p.add(tmpButton);
    tmpButton = new Button(DEFAULT_STRING);
    tmpButton.addActionListener(al);
    p.add(tmpButton);
    add("South",p);
  }
  public void updateLayout(){
    statLabel.invalidate();
    validateTree();
  }
  public void showAgain(){
      old_qserver = QtfServer.getText();
      old_sserver = StfServer.getText();
      old_qport = QtfPort.getText();
      old_sport = StfPort.getText();
      old_pwd = Passwd.getText();
//      old_available = db_available.getText();
  //    old_selected = db_selected.getText();
      old_available = db_available.getItems();
      old_selected = db_selected.getItems();
    setVisible (true);
  }
  
  protected void changeDB(){
    String[] selected = db_selected.getItems();
    String request = new String();
    for(int i = 0; i < selected.length; i++)
      request += selected[i] + ',';

    if ( gr.getNewRequest(DB_SELECT+request) > 0)
      setVisible(false);
    else{
      statLabel.setText("ERROR: No sources selected");
      updateLayout();
      return;
    }
  }
  
  protected void handleSubmit(){
    if( !(gr.getSockInfo().equals(QtfServer.getText().trim())) ||
	!(gr.getPort().equals(QtfPort.getText().trim())) ){
      /**** Netscape Security stuff */
      try {
	PrivilegeManager.enablePrivilege ("UniversalConnect");
	
      }
      catch (netscape.security.ForbiddenTargetException err) {
	System.out.print("*Access Denied to connect, exiting*\n");
	statLabel.setText("Access Denied to connect*");
	return;
      }
	 
      /* ******************************/
      int newPort = getPort(QtfPort.getText().trim());
      if(newPort > 65535 || newPort <= 0){
	newPort = getPort(gr.getPort());
	QtfPort.setText(Integer.toString(newPort));
	return;
      }
      
      try{
	InetAddress Qserv = InetAddress.getByName(QtfServer.getText().trim());
      }
      catch(UnknownHostException e){
	statLabel.setText("Invalid Query host entered");
	updateLayout();
	return;
      }
      try{
	InetAddress Sserv = InetAddress.getByName(StfServer.getText().trim());
      }
      catch(UnknownHostException e){
	statLabel.setText("Invalid Submit host entered");
	updateLayout();
	return;
      }
      irrw.changeServer(QtfServer.getText().trim(), newPort);
      newPort = getPort(StfPort.getText().trim());
      if(newPort > 65535 || newPort <= 0){
	newPort = getPort(gr.getPort());
	StfPort.setText(Integer.toString(newPort));
	return;
      }
      gr = irrw.gr;
      updateDbList();
      setVisible(false);
    }
    else
      changeDB();
    /*setVisible(false);*/
  }

  /* called to rebuild the list of available databases after a server change */
  public void updateDbList(){
    int bytes = gr.getNewRequest(DB_RESET);
    bytes = gr.getNewRequest(DB_REQUEST);
    if (bytes < 1) return;
    String s = gr.getBuffer();
   
    if (s == null) return;
     //db_selected.setText(s.replace(',', '\n'));
    StringTokenizer strTok = new StringTokenizer(s, ",", false);
    while(strTok.hasMoreTokens())
	db_selected.addItem(strTok.nextToken());
   
  }
  
  
  public int getPort(String p){
    int i;
    try{
      i = Integer.valueOf(p).intValue();
    }
    catch(NumberFormatException f){
      i = getPort(gr.getPort());
    }
    return i;
  }
    
    public void Close(){
	super.Close();
    }

   class InnerActionListener implements ActionListener {
      public void actionPerformed (ActionEvent e) {
         String label = e.getActionCommand ();
         
         if (label.equals (CLOSE_STRING)) {
	     QtfServer.setText(old_qserver);
	     StfServer.setText(old_sserver);
	     QtfPort.setText( old_qport );
	     StfPort.setText(old_sport);
	     Passwd.setText(old_pwd);
	 //    db_selected.setText(old_selected);
	  //   db_available.setText(old_available);
	     db_selected.removeAll();
	     db_available.removeAll();
	     for(int i = 0; i < old_selected.length; i++)
	       db_selected.addItem(old_selected[i]);
	     for(int i = 0; i < old_available.length; i++)
	       db_available.addItem(old_available[i]);

	     statLabel.setText("");
	     setVisible (false);
         }
         else if (label.equals (SUBMIT_STRING)) {
	   statLabel.setText("");
	     handleSubmit();
         }
	 else if (label.equals ( DEFAULT_STRING)) {
	   QtfServer.setText(irrw.DEFAULT_SERVER);
	   QtfPort.setText(Integer.toString(irrw.DEFAULT_PORT));
	   StfServer.setText (irrw.DEFAULT_SERVER);
	   StfPort.setText (DEFAULT_SPORT);
	   //db_available.setText("");
	   db_available.removeAll();
	   updateDbList();
	   updateLayout();
	 }
	 else if (label.equals ( "-->" )){/*
	   String s = db_available.getSelectedText();
	   System.out.print("Selected: " + s + "\n");
	   StringTokenizer sst = new StringTokenizer(s, " \t\r\n", false);
	   //String s;
	   while(sst.hasMoreTokens()){
	     s = sst.nextToken();
	     System.out.print("Token: " + s + "\n");
	     String str = db_available.getText();
	     System.out.print("Available: " + str + "\n");
	     StringTokenizer st = new StringTokenizer(str, " \t\r\n", false);
	     db_available.setText("");
	     //String str;
	     while(st.hasMoreTokens()){
	       str = st.nextToken();
	       System.out.print("Compare to token: " + str + "\n");
	       if(str.equals(s))
		 db_selected.append(s + "\n");
	       else
		 db_available.append(str + "\n");
	     }
	   }
*/
	   String[] s = db_available.getSelectedItems();
	   for(int i = 0; i < s.length; i++){
	     db_available.remove(s[i]);
	     db_selected.addItem(s[i]);
	   }
	 }
	 else if (label.equals ("<--" )){
/*	   StringTokenizer sst = new
StringTokenizer(db_selected.getSelectedText().trim());
	   String s;
	   while(sst.hasMoreTokens()){
	     s = sst.nextToken();
	     StringTokenizer st = new StringTokenizer(db_selected.getText());
	     db_selected.setText("");
	     String str;
	     while(st.hasMoreTokens()){
	       str = st.nextToken();
	       if(str.equals(s))
		 db_available.append(s + "\n");
	       else
		 db_selected.append(str + "\n");
	     } 
	   } */
	   String[] s = db_selected.getSelectedItems();
	   for(int i = 0; i < s.length; i++){
	     db_selected.remove(s[i]);
	     db_available.addItem(s[i]);
	   }
	   
	 } 
      }
   }
   
   class InnerWindowAdapter extends WindowAdapter {
      public void windowClosing (WindowEvent e) {
	QtfServer.setText(old_qserver);
	StfServer.setText(old_sserver);
	QtfPort.setText( old_qport );
	StfPort.setText(old_sport);
	Passwd.setText(old_pwd);
	//db_selected.setText(old_selected);
	//db_available.setText(old_available);
	db_selected.removeAll();
	db_available.removeAll();
	for(int i = 0; i < old_selected.length; i++)
	  db_selected.addItem(old_selected[i]);
	for(int i = 0; i < old_available.length; i++)
	  db_available.addItem(old_available[i]);
	
	statLabel.setText("");
	setVisible (false);
      }
   }   
   
} // End of class definition
