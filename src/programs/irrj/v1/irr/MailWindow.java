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
 * MailWindow
 * 
 * <functionality>
 * 
 * Modification History:
 *
 * May 1999 
 * Removed Email support, added auth selector, better edit options
 * Jon Poland
 *
 * @version 1.0
 * @date 
 * @author Aaron Klink.
 */


import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.net.*;
import java.io.*;
import ipma.Window.*;
import ipma.Util.*;
import ipma.Help.*;
//import debug.*;
import netscape.security.PrivilegeManager;

public class MailWindow extends CWindow{
  
    private GetRequest gr;
    private TextArea ta;
    private TextArea ta_lower;
    private OptionsMenu opt_window;
    private Font f = new Font("Courier", Font.PLAIN, 12);
    private boolean split = false;
  
    private Choice TChoice = new Choice();

    // Strings for the submit by menu

  static final String  MULTI_WINDOW_TITLE = "Edit Objects Window";
  static final String  NEW_WINDOW_TITLE = "New Object";

    // Labels and textfields 
  
    Label     authLabel = new Label("Authorization Type:");
    Choice  authChoice = new Choice();
 
    // Socket crap
    private Socket s;
    private PrintWriter ps;
    private BufferedReader in;
    private InetAddress ia;
    
    // Listener stuff
    protected ActionListener al = new InnerActionAdapter();
   
    
    // String constants
    private String AUTH_PASS = "Auto-append my Mntner password";
    private String AUTH_PGP = "Sign using my local PGP binary";
    private String AUTH_NONE = "None, I've done it already";
    private String SUBMIT_STRING = "Submit";
    private String CLOSE_STRING  = "Close";
    private String MENU_EDIT = "Edit";
    private String MENU_ITEM_CLEAR = "Clear edit window";
    private String MENU_IMPORT = "Import Template";
    private String MENU_HELP = "Help";
    private String MENU_PGP_HELP = "PGP Help";
    private String MENU_DEL_ALL = "Delete ALL these objects";
    private String MENU_OBJECTS = "Objects";       
    private String DEL_STRING = "delete: TCP IRRj Deletion\n";
    private String MENU_UNSPLIT = "Unsplit screen";
    private String MENU_SPLIT = "Split screen";
    private String MENU_SEP_WIN = "Open results in seperate window"; 
    private String MENU_IMPORT_FILE = "Import from file";
    private String MENU_EXPORT_FILE = "Export to file";
    
    private CHelpWindowParent    Enc_Help;
    private CHelpWindowParent    Help;
    private String ENCRYPT_HELP[] = { 
        "   To Use PGP: \n",
	"1st, Make sure the setting in the options menu are correct.\n",
	"2nd, Set the \"Authentification\" to \"PGP\".\n",
	"3rd, IRRj will try to invoke your local PGP binary.\n",
	"4th, If successful, IRRj will send the signed submission to the IRR.\n",
	"5th, The results of the submission will come back in the lower half\n",
	"        of the window\n.",
	"IRRj assumes that if your binary is called \"pgps\" you're using PGP v5\n ",
	"and if not, it assumes you're using PGP v2.6 (the two version have\n",
	"different switches).  A platform that does not have a commandline\n",
	"based PGP distribution (.i.e. Windows), will need to use the \"Import\"\n",
	"and \"Export\" functions with \"Authentification\" set to \"None\"\n"
    };
    private String ML_WIN_HELP[] = {
      "Right clicking on objects in the main window appends\n",
      "them to the end of the \"Edit objects\" window.  Once objects\n",
      "are there, you just need to click \"Submit\" to send them.\n",
      "Make sure you choose the right authorization type.  IRRj will\n",
      "automatically append you maintainer password from the OptionsMenu\n",
      "when you choose \"Password\" authentification.  If you choose \"PGP\",\n",
      "when you click \"Submit\", another help menu will appear\n",
      "\"Import Template\" queries the current query server and returns the\n",
      "specified object template.\n",
      "The \"Objects\" Menu has three useful functions. \"Delete all\" will\n",
      "mark all the objects in the current edit window for deletion.  You will\n",
      "still need to submit with proper authentification to actually complete\n",
      "the deletion.  \"Import\" and \"Export\" are mainly for PGP, and import\n",
      "or export the current contents of the editor window to or from file\n"
    };

    /* Templates available */
    private String TEMPLATE_ONE = "route";
    private String TEMPLATE_TWO = "mntner";
    private String TEMPLATE_THREE = "aut-num";
    private String TEMPLATE_FOUR = "inetnum";
    private String TEMPLATE_FIVE = "inet6num";
    private String TEMPLATE_SIX = "inet-rtr";
    private String TEMPLATE_SEVEN = "person";
    private String TEMPLATE_EIGHT = "role";
    private String TEMPLATE_NINE = "as-macro";
    private String TEMPLATE_TEN = "community";
 

    /* 
       End of variable declarations
       Beginning of real code.
       
    */
    
    public MailWindow(String s, GetRequest g_r, OptionsMenu o){
	
	super.setTitle(MULTI_WINDOW_TITLE);  

	ta = new TextArea(30,60);
	ta_lower = new TextArea(15,60);
	ta_lower.setEditable(false);

	opt_window = o;
	BuildLayout();

	authChoice.add(AUTH_PASS);
	authChoice.add(AUTH_PGP); 
	authChoice.add(AUTH_NONE);
	addWindowListener(new InnerWindowAdapter ());
	ta.setText(s);
	gr = g_r;
	pack();

    }
    
    public MailWindow(GetRequest g_r, OptionsMenu o){
	
	super.setTitle(NEW_WINDOW_TITLE);  

	ta = new TextArea(30,60);
	ta_lower = new TextArea(15,60);
	ta_lower.setEditable(false);


	opt_window = o;
	BuildLayout();
	authChoice.add(AUTH_PASS);
	authChoice.add(AUTH_PGP); 
	authChoice.add(AUTH_NONE);
	addWindowListener(new InnerWindowAdapter ());
	gr = g_r;
	pack();
	show();
    }
    
   
    protected void saveFile(){
      /**** Netscape Security stuff */
       try {
	 PrivilegeManager.enablePrivilege ("UniversalFileWrite");
	
	 FileDialog fileD = new FileDialog(this,"Save Objects",FileDialog.SAVE);
	fileD.show();
	String currentFile = fileD.getFile();
	
	try {
	    FileOutputStream fos = new FileOutputStream(currentFile);
	    byte data[] = ta.getText().trim().getBytes();
	    fos.write(data);
	    fos.close();
	}
	catch(Exception e){System.out.println(e.getMessage());}
	
       }
       catch (netscape.security.ForbiddenTargetException err) {
	 System.out.print("*Universal Save file denied*\n");
	 ta_lower.setText("*Universal Save file denied*\n");
       }
       /******************************/
	
    }
    
    protected void readFile(){
	FileDialog fileD = new FileDialog(this,"Load from File",FileDialog.LOAD);
	fileD.show();
	 /**** Netscape Security stuff */
	try {
	  PrivilegeManager.enablePrivilege ("UniversalFileRead");
	   try {
	    FileInputStream fis = new FileInputStream(fileD.getFile());
	    byte [] data = new byte [ fis.available() ];
	    fis.read(data);
	    fis.close();
	    ta.setText(new String(data) + "\n\n");
	}
	catch (Exception e ) { }
	}
	catch (netscape.security.ForbiddenTargetException err) {
	  System.out.print("*Universal Read file forbidden*\n");
	  ta_lower.setText("*Universal Save file denied*\n");
	}
	/******************************/
    }
    
    protected void handleSubmit(){
	String text = null;
	/* make sure submit isn't emty */
	if(ta.getText().trim() == "")
	  return;
	/* clear old results */
	ta_lower.setText("");
	String authType = authChoice.getSelectedItem();
	/* Check auth type */
	/* if crypt password */
	if(authType.equals(AUTH_PASS)){
	  /* append  passwd to start */
	  if(opt_window.Passwd.getText().trim().equals("")){
	    opt_window.statLabel.setText("Enter a mntner password and submit again");
	    opt_window.updateLayout();
	    opt_window.showAgain();
	    return;
	  }
	  text = new String("password: " + opt_window.Passwd.getText().trim() + "\n" + ta.getText().trim());
	}
	/* if pgp auth */
	else if(authType.equals(AUTH_PGP)){
	  text = handlePgp();
	  if(text == null)
	    return;
	}
	else
	  text = ta.getText().trim();
	/* common code */
       /**** Netscape Security stuff */
	try {
	  PrivilegeManager.enablePrivilege ("UniversalConnect");
	 
	  String SServer = opt_window.StfServer.getText().trim();
	  int SPort = Integer.valueOf(opt_window.StfPort.getText().trim()).intValue();
	  GetRequest gf = new GetRequest(SServer, SPort, 1);
	  if(gf.error != 0){
	    opt_window.statLabel.setText("Problem connecting to submit server, try again");
	    opt_window.updateLayout();
	    opt_window.showAgain();
	    return;
	  }
	  ta_lower.setText(gf.getRawRequest(text + "\n!q"));
	  //System.out.print(text);
	  gf.quit();
	  if(split == false){
	    split = true;
	    UpdateLayout();
	  }
	}
	catch (netscape.security.ForbiddenTargetException err) {
	  System.out.print("*Access denied for connection*\n");
	  ta_lower.setText("*Access denied for connection*\n");
	}
	/******************************/
    }
    
  public String handlePgp(){
    String text = new String();
    Process pgp = null;
    String temp_uid =  opt_window.pgpUid.getText().trim();
    String temp_exec = opt_window.pgpExec.getText().trim();
    String temp_pwd = opt_window.pgpPwd.getText().trim();
    String temp_path = opt_window.pgpPath.getText().trim();
    DataInputStream s_err = null;
    DataInputStream s_in = null;
    PrintWriter s_out = null;

    String EXEC_STRING = null;
      if(temp_uid.equals("") || temp_exec.equals("") || temp_pwd.equals("") || temp_path.equals("")){
	opt_window.statLabel.setText("Make sure all pgp attributes are filled");
	opt_window.updateLayout();
	opt_window.showAgain();
	return null;
      }

      /**** Netscape Security stuf */
       try {
	 PrivilegeManager.enablePrivilege ("UniversalExecAccess");
       }
       catch (netscape.security.ForbiddenTargetException err) {
	 System.out.print("*Access denied for running pgp, exiting*\n");
	 return null;
       }
       try {
	 PrivilegeManager.enablePrivilege ("UniversalPropertyWrite");
       }
       catch (netscape.security.ForbiddenTargetException err) {
	 System.out.print("*Access denied for setting variables*\n");
	 ta_lower.setText("*Access deined for setting variables*\n");
	 return null;
       }
       
       /*****************************/
								
      try{
	/* PGP v5 */
	if(opt_window.pgpExec.getText().trim().endsWith("pgps"))
	  EXEC_STRING = new String(temp_exec + " -u " + temp_uid + " -fta");
	/* assume pgp 2.6 */
	else
	  EXEC_STRING = new String(temp_exec + " -fsta -u " + temp_uid);
	
	String ENV_STRING[] = {"PGPPASS="+temp_pwd, "PGPPATH="+temp_path};
	/* run the external process */
	pgp = Runtime.getRuntime().exec(EXEC_STRING, ENV_STRING);
	Runtime.getRuntime().gc();
	if(pgp == null)
	  return null;
	/* Open streams to it */
	s_in = new DataInputStream(pgp.getInputStream());
	s_out = new PrintWriter(pgp.getOutputStream());
	s_err = new DataInputStream(pgp.getErrorStream());
	/* Send the input */
	s_out.print(ta.getText().trim());
	s_out.close(); 
	/* get output, the messy part */
	int numTimes = 0;
	while(s_in.available() == 0){
	  numTimes++;
	  try{
	    Thread.sleep(150);
	    if( pgp.exitValue() != 0 ){
	      byte err_msg[] = new byte[s_err.available()];
	      s_err.read(err_msg);
	      String m;
	      StringTokenizer st = new StringTokenizer(new String(err_msg));
	      while(st.hasMoreTokens()){
		m = st.nextToken();
		if(m.equals("keyring")){
		  opt_window.statLabel.setText("Problem with keyrings, check PGP Path");
		  opt_window.updateLayout();
		  opt_window.showAgain();
		  s_in.close();
		  s_err.close();
		  pgp.destroy();
		  return null;
		}
	      }
	      opt_window.statLabel.setText("Could not find Uid " + temp_uid);
	      opt_window.updateLayout();
	      opt_window.showAgain();
	       s_in.close();
	       s_err.close();
	       pgp.destroy();
	       return null;
	    }
	  }
	  catch( IllegalThreadStateException e ){
	    System.out.print(".");
	    if(numTimes > 10){
	      new GeneralErrorWindow("Problem with external pgp process, killing process (check password)\n");
	      pgp.destroy();
	      return null;
	    }
	  }
	  catch(InterruptedException g){
	    System.out.print(g);
	  }
	}
	/* actually get the output */
	byte ret[] = null;
	while(s_in.available() > 0){
	  ret = new byte[s_in.available()];
	  s_in.read(ret);
	  text = text + new String(ret);
	} 
	s_in.close();
	s_err.close();
      }
      catch(IOException e){
	System.out.print(e);
	opt_window.statLabel.setText("Could not find " + temp_exec);
	opt_window.updateLayout();
	opt_window.showAgain();
	return null;
      }
      catch(SecurityException e){
	System.out.print("Security exception when calling external pgp\n");
      }
      /* if the pgp process is running, it shouldn't be, kill it */
      /* if some other error occurred, make user aware */
      try{
	if( pgp.exitValue() != 0 ){
	  byte err_msg[] = new byte[s_err.available()];
	  s_err.read(err_msg);
	  new GeneralErrorWindow(new String(err_msg));
	  pgp.destroy();
	  return null;
	}
      }
      catch( IllegalThreadStateException e ){
	System.out.print("killed external proc\n");  
	pgp.destroy();
      }
      catch( IOException e){}
    
      return text;
  }

  
 public final void BuildMenu() {
      
      Menu menu;
      MenuItem menuItem;
      Menu submenu;
      MenuBar menubar  = new MenuBar();
      
      submenu = new Menu(MENU_IMPORT);

      menuItem = new MenuItem(TEMPLATE_ONE);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_TWO);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_THREE);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_FOUR);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_FIVE);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_SIX);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_SEVEN);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_EIGHT);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_NINE);
      menuItem.addActionListener(al);
      submenu.add(menuItem);
      menuItem = new MenuItem(TEMPLATE_TEN);
      menuItem.addActionListener(al);
      submenu.add(menuItem);

      menu = new Menu(MENU_EDIT);
      menuItem = new MenuItem(MENU_ITEM_CLEAR);
      menuItem.addActionListener(al);
      menu.add (menuItem);
      menu.addSeparator();
      menuItem = new MenuItem(MENU_SPLIT);
      menuItem.addActionListener(al);
      menu.add (menuItem);
      menuItem = new MenuItem(MENU_UNSPLIT);
      menuItem.addActionListener(al);
      menu.add (menuItem);
      menuItem = new MenuItem(MENU_SEP_WIN);
      menuItem.addActionListener(al);
      menu.add (menuItem);
      
      menubar.add(menu);
      menubar.add(submenu);
      
      menu = new Menu(MENU_OBJECTS);
      menuItem = new MenuItem(MENU_DEL_ALL);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      menuItem = new MenuItem(MENU_IMPORT_FILE);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      menuItem = new MenuItem(MENU_EXPORT_FILE);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      menubar.add(menu);
      
      
      Help = new CHelpWindowParent("Editor Window Help", ML_WIN_HELP);
      Enc_Help = new CHelpWindowParent("PGP Submission Help", ENCRYPT_HELP);
      menu = new Menu(MENU_HELP);
      menuItem = new MenuItem(MENU_HELP);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      menuItem = new MenuItem(MENU_PGP_HELP);
      menuItem.addActionListener(al);
      menu.add(menuItem);
      
      menubar.setHelpMenu(menu);
      setMenuBar(menubar);

   }
   
    protected void BuildLayout(){
	Panel p;
	p = new Panel();
	setLayout(new BorderLayout());
	
	ta.setFont(f);
	BuildMenu();
	Panel two = new Panel();
	two.setLayout(new BorderLayout());
	two.add("Center", ta);
	if(split == true)
	  two.add("South", ta_lower);
	add("Center", two);
	
	GridBagLayout       g = new GridBagLayout();
	GridBagConstraints  gc = new GridBagConstraints();
	
	p.setLayout(g);
	
	gc.fill      = GridBagConstraints.NONE;
	gc.gridwidth = GridBagConstraints.REMAINDER; 
	gc.gridx     = 0;
	gc.anchor    = GridBagConstraints.WEST;

	Panel serverP = new Panel();
	serverP.add(authLabel);
	serverP.add(authChoice);
	g.setConstraints(serverP,gc);
	p.add(serverP);
	
	Panel butP = new Panel();
	Button tmpButton = new Button(SUBMIT_STRING);
	tmpButton.addActionListener(al);
	butP.add(tmpButton);
	tmpButton = new Button(CLOSE_STRING);
	tmpButton.addActionListener(al);
	butP.add(tmpButton);
	p.add(butP);
	add("South",p);
    }

  protected void UpdateLayout(){
    Dimension dim = getSize();
    remove(getComponentAt(dim.width/2, dim.height/2));
	Panel far = new Panel();
	far.setLayout(new BorderLayout());
	far.add("Center", ta);
	if(split == true)
	  far.add("South", ta_lower);
	add("Center", far);

	validateTree();
  }
   
 
  public void changeServer(GetRequest g_r){
	gr = g_r;
  }
  
    protected void handleTemplate(String TChoice){
	String req = new String();
	int bytes = -1;

	appendText(gr.getRawRequest("-t " + TChoice) + "\n");
    }
    
    public void Close(){
    
      ta.setText("");
      super.Close();
    }
    
    protected void appendText(String str){
	ta.append(str);
    }
    
   
    class InnerActionAdapter implements ActionListener {
	public void actionPerformed (ActionEvent e) {
	    String label = e.getActionCommand ();
	    /* if submit button */
	    if(label.equals(SUBMIT_STRING))
		handleSubmit();
	    
	    else if (label.equals (CLOSE_STRING)) {
		setVisible (false);
		ta.setText("");
	    }
	    /* if a menu item */
	    /* if clear */
	    else if (label.equals (MENU_ITEM_CLEAR)) {
		ta.setText("");
	    }
	    else if (label.equals (MENU_DEL_ALL)) {
	      // add the word DELETE after each
	      int count = 0;
	      String m, lastToken = "j";
	      StringTokenizer st = new StringTokenizer(ta.getText(), "\n", true);
	      while(st.hasMoreTokens()){
	        m = st.nextToken();
		count += m.length();
		if(m.equals("\n") && lastToken.equals("\n")){
		  ta.insert(DEL_STRING, count-1);
		  count += DEL_STRING.length();
		}
		lastToken = m;
	      }
	    }
	    else if ( label.equals (MENU_SPLIT)){
	      split = true;
	      UpdateLayout();
	    }
	    else if (label.equals (MENU_UNSPLIT)){
	      split = false;
	      UpdateLayout();
	    }
	    else if (label.equals (MENU_IMPORT_FILE)){
	       readFile();
	    }
	    else if (label.equals (MENU_EXPORT_FILE)){
	       saveFile();
	    }
	    else if (label.equals (MENU_SEP_WIN)) {
	      new GeneralErrorWindow(ta_lower.getText());
	    }
	    else if (label.equals (MENU_HELP)) {
	      Help.ShowHelpWindow();
	    }
	    else if (label.equals (MENU_PGP_HELP)){
	      Enc_Help.ShowHelpWindow();
	    }
	    else
	      handleTemplate(label);
	}
    }
    
  
    class InnerWindowAdapter extends WindowAdapter {
	public void WindowClosing (WindowEvent e) {
	    setVisible (false);
	}
    }
    
    
   
} // End of class definition
