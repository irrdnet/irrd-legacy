import java.awt.event.*;
import java.awt.*;
import javax.swing.*;
import java.util.*;
import javax.swing.text.*;
import java.awt.BorderLayout;
import java.awt.Color;
import java.net.*;
import javax.swing.undo.*;
import javax.swing.event.*;
import java.awt.datatransfer.*;
import java.io.*;
import java.text.*;

public class EditorWindow extends JFrame implements ActionListener{
    
    JTextArea topta = new JTextArea(10, 25);
    JTextArea botta = new JTextArea(10, 25);
    JSplitPane jsp = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
    JScrollPane topScroll = new JScrollPane(topta);
    JScrollPane botScroll = new JScrollPane(botta);
    private String AUTH_PASS = "CRYPT-PW";
    private String AUTH_PGP = "PGP-FROM";
    private String AUTH_NONE = "NONE";
    private String AUTH_DIST = "PGP(RPS-AUTH-USER)";
    private String AUTH_REP = "PGP(RPS-AUTH-REPOSITORY)";
    JComboBox authChoice = new JComboBox();
    Font smallFont = new Font("Monospaced", Font.PLAIN, 12);
    protected UndoManager undoManager = new UndoManager();
    protected JFileChooser jfc = new JFileChooser();
    
    public EditorWindow(){
	super("Edit WIndow");
	initCommon();
	pack();
	setVisible(true);
    }

    public EditorWindow(String topContents){
	super("Edit WIndow");
	topta.setText(topContents);
	initCommon();
	pack();
	setVisible(true);
    }

    public void initCommon(){
	addWindowListener(new WindowAdapter(){
		public void windowClosing(WindowEvent e){
		    dispose();
		} 
	    });
	buildLayout();
	botta.setFont(smallFont);
	topta.setFont(smallFont);
	topta.setTabSize(5);
	botta.setEditable(false);
	botta.setBackground(Color.lightGray);
	jsp.setOneTouchExpandable(true);
	authChoice.addItem(AUTH_PASS);
	authChoice.addItem(AUTH_PGP);
	authChoice.addItem(AUTH_NONE);
	authChoice.addItem(AUTH_DIST);
	authChoice.addItem(AUTH_REP);
	authChoice.setFont(smallFont);
	topta.getDocument().addUndoableEditListener(
	     new UndoableEditListener() {
		     public void undoableEditHappened(UndoableEditEvent e) {
			 undoManager.addEdit(e.getEdit());
		     }
		 });
    }

    public void buildLayout(){
	buildMenuBar();
	getContentPane().setLayout(new BorderLayout());
	jsp.setTopComponent(buildTopPanel());
	jsp.setBottomComponent(buildBottomPanel());
	getContentPane().add(jsp);
	
	JPanel bottomPanel = new JPanel();
	JLabel lbl = new JLabel("Authorization type:");
	lbl.setFont(smallFont);
	bottomPanel.add(lbl);
	bottomPanel.add(authChoice);
	JButton okButton = new JButton("Submit");
	getRootPane().setDefaultButton(okButton);
	okButton.setForeground(Color.blue);
	okButton.addActionListener(this);
	okButton.setFont(smallFont);
	bottomPanel.add(okButton);
	getContentPane().add(bottomPanel, BorderLayout.SOUTH);
    }
    
    public void buildMenuBar(){
	JMenuBar topMenuBar = new JMenuBar();
	setJMenuBar(topMenuBar);
	JMenu fileMenu = new JMenu("File");
	topMenuBar.add(fileMenu);
	JMenu editMenu = new JMenu("Edit");
	topMenuBar.add(editMenu);
	
	JMenuItem saveMenuItem = new JMenuItem("Save...");
	fileMenu.add(saveMenuItem);
	saveMenuItem.addActionListener(this);
	JMenuItem loadMenuItem = new JMenuItem("Load...");
	fileMenu.add(loadMenuItem);
	loadMenuItem.addActionListener(this);
	JMenuItem closeMenuItem = new JMenuItem("Close");
	fileMenu.addSeparator();
	fileMenu.add(closeMenuItem);
	closeMenuItem.addActionListener(this);

	JMenuItem undoMenuItem = new JMenuItem("Undo");
	editMenu.add(undoMenuItem);
	undoMenuItem.addActionListener(this);
	JMenuItem redoMenuItem = new JMenuItem("Redo");
	editMenu.add(redoMenuItem);
	redoMenuItem.addActionListener(this);
	editMenu.addSeparator();
	JMenuItem copyMenuItem = new JMenuItem("Copy");
	editMenu.add(copyMenuItem);
	copyMenuItem.addActionListener(this);
	JMenuItem pasteMenuItem = new JMenuItem("Paste");
	editMenu.add(pasteMenuItem);
	pasteMenuItem.addActionListener(this);
	editMenu.addSeparator();
	JMenuItem wizardMenuItem = new JMenuItem("Object Wizard...");
	editMenu.add(wizardMenuItem);
	wizardMenuItem.addActionListener(this);
    }
    
    public JPanel buildTopPanel(){
	JPanel tPanel = new JPanel();
	tPanel.setLayout(new BorderLayout());
	JToolBar bar = new JToolBar(JToolBar.VERTICAL);
	bar.setFloatable(false);
	
	JButton undoButton = new JButton((ImageIcon)IRRj.iconHash.get("undoIcon"));
	undoButton.setToolTipText("Undo");
	undoButton.setActionCommand("Undo");
	undoButton.addActionListener(this);	

	JButton redoButton = new JButton((ImageIcon)IRRj.iconHash.get("redoIcon"));
	redoButton.setToolTipText("Redo");
	redoButton.setActionCommand("Redo");
	redoButton.addActionListener(this);	
	
	JButton copyButton = new JButton((ImageIcon)IRRj.iconHash.get("copyIcon"));
	copyButton.setToolTipText("Copy");
	copyButton.setActionCommand("Copy");
	copyButton.addActionListener(this);
	
	JButton pasteButton = new JButton((ImageIcon)IRRj.iconHash.get("pasteIcon"));
	pasteButton.setToolTipText("Paste");
	pasteButton.setActionCommand("Paste");
	pasteButton.addActionListener(this);

	JButton saveButton = new JButton((ImageIcon)IRRj.iconHash.get("saveIcon"));
	saveButton.setToolTipText("Save");
	saveButton.setActionCommand("Save...");
	saveButton.addActionListener(this);

	JButton loadButton = new JButton((ImageIcon)IRRj.iconHash.get("loadIcon"));
	loadButton.setToolTipText("Load");
	loadButton.setActionCommand("Load...");
	loadButton.addActionListener(this);

	bar.add(undoButton);
	bar.add(redoButton);
	bar.add(copyButton);
	bar.add(pasteButton);
	bar.add(saveButton);
	bar.add(loadButton);
      
	tPanel.add(bar, BorderLayout.WEST);
	tPanel.add(topScroll);
	return tPanel;
    }

    public JPanel buildBottomPanel(){
	JPanel tPanel = new JPanel();
	tPanel.setLayout(new BorderLayout());
	JToolBar bar = new JToolBar(JToolBar.VERTICAL);
	bar.setFloatable(false);

	JButton copyButton = new JButton((ImageIcon)IRRj.iconHash.get("copyIcon"));
	copyButton.setToolTipText("Copy");
	copyButton.setActionCommand("CopyBottom");
	copyButton.addActionListener(this);
	bar.add(copyButton);
	
	tPanel.add(bar, BorderLayout.WEST);
	tPanel.add(botScroll);

	return tPanel;
    }
	
    protected void handleSubmit(){
	String text = null;
	/* make sure submit isn't emty */
	if(topta.getText().trim() == "")
	  return;
	/* clear old results */
	botta.setText("");
	String authType = (String)authChoice.getSelectedItem();
	/* Check auth type */
	/* if crypt password */
	if(authType.equals(AUTH_PASS)){
	  /* append  passwd to start */
	  if(IRRj.optWin.options.mps.trim().equals("")){
	    IRRj.optWin.statLabel.setText("Enter a Maintainer Password and submit again");
	    IRRj.optWin.tabbedPane.setSelectedIndex(2);
	    IRRj.optWin.setVisible(true);
	    return;
	  }
	  text = new String("password: " + IRRj.optWin.options.mps.trim() + "\n" + topta.getText().trim());
	}
	/* if pgp auth */
	else if(authType.equals(AUTH_PGP)){
	    text = handlePgp(false, topta.getText().trim());
	    if(text == null)
		return;
	    //else
		//		botta.setText(text);
		//return;
		//}
	}
	// STILL NEEDS "transaction-submit-begin: and ...end"
	else if(authType.equals(AUTH_DIST)){
	   text = handlePgp(true, topta.getText().trim() + "\n\n" + timeStamp() + "\n\n");
	   if(text == null)
		return;
	   else{
		botta.setText(topta.getText().trim());
		botta.append("\n\n" + timeStamp() + "\n\n");
		botta.append("signature:\n");
		StringTokenizer strtok = new StringTokenizer(text, "\n", true);
		String token;
		int ns = 0;
		while(strtok.hasMoreTokens()){
		    token = strtok.nextToken();
		    if(token.equals("\n"))
			ns++;
		    else
			ns = 0;
		    if(ns > 1 && token.equals("\n"))
			botta.append("+" + token);
		    else if(!token.equals("\n"))
			botta.append("+" + token + "\n");
		}
		return;
	    }
	} 
	else if(authType.equals(AUTH_REP)){
	    // from above
	    text = handlePgp(true, topta.getText().trim() + "\n\n" + timeStamp() + "\n\n");
	    if(text == null)
		return;
	    else{
		botta.setText(topta.getText().trim());
		botta.append("\n\n" + timeStamp() + "\n\n");
		botta.append("signature:\n");
		StringTokenizer strtok = new StringTokenizer(text, "\n", true);
		String token;
		int ns = 0;
		while(strtok.hasMoreTokens()){
		    token = strtok.nextToken();
		    if(token.equals("\n"))
			ns++;
		    else
			ns = 0;
		    if(ns > 1 && token.equals("\n"))
			botta.append("+" + token);
		    else if(!token.equals("\n"))
			botta.append("+" + token + "\n");
		}
	    }
	    topta.setText(botta.getText());
	    // end above
	    String encStr = "transaction-label: "+ getDb(botta.getText()) 
		+ "\nsequence: 2\n" + timeStamp() + 
		"\nintegrity: authorized\n\n" + topta.getText().trim() + "\n\n" +
		"repository-signature: " + getDb(botta.getText()) + "\n";
	    text = handlePgp(true, encStr);
	    if(text == null)
		return;
	    else{
		String fullString = encStr;
		fullString += "signature:\n";
		StringTokenizer strtok = new StringTokenizer(text, "\n", true);
		String token;
		int ns = 0;
		while(strtok.hasMoreTokens()){
		    token = strtok.nextToken();
		    if(token.equals("\n"))
			ns++;
		    else
			ns = 0;
		    if(ns > 1 && token.equals("\n"))
			fullString += ("+" + token);
		    else if(!token.equals("\n"))
			fullString += ("+" + token + "\n");
		}
		botta.setText("transaction-begin: " + fullString.length() 
			      + "\ntransfer-method: plain\n\n");
		botta.append(fullString);
		text = botta.getText();
	    }
	}
	else
	  text = topta.getText().trim();

	String SServer = IRRj.optWin.options.sss.trim();
	int SPort = Integer.valueOf(IRRj.optWin.options.sps.trim()).intValue();
	GetRequest gf = new GetRequest(SServer, SPort, 1);
	if(gf.error != 0){
	  IRRj.optWin.statLabel.setText("Problem connecting to submit server, try again");
	  IRRj.optWin.tabbedPane.setSelectedIndex(0);
	  IRRj.optWin.setVisible(true);
	  return;
	}
	botta.setText(gf.getRawRequest(text + "\n!q"));
	gf.quit();
    }
   
    public String getDb(String scanText){
	StringTokenizer strtok = new StringTokenizer(scanText, "\n");
	String str;
	while(strtok.hasMoreTokens()){
	    str = strtok.nextToken();
	    if(str.startsWith("source:"))
		return str.substring(7).trim();
	}
	
	return null;
    }

    public void actionPerformed(ActionEvent e){
	String event = e.getActionCommand();
	if(event.equals("Submit"))
	    handleSubmit();
	else if(event.equals("Paste"))
	    topta.paste();
	else if(event.equals("Undo")){
	    try{
		undoManager.undo();
	    }
	    catch (CannotUndoException cre) {}
	}
	else if(event.equals("Redo")){
	    try{
		undoManager.redo();
	    }
	    catch (CannotRedoException cre) {}
	}    
	else if(event.equals("Close"))
	    dispose();
	else if(event.equals("Copy")){
	    String s = topta.getSelectedText(); 
	    if(s != null) {
		StringSelection ss = new StringSelection(s); 
		this.getToolkit().getSystemClipboard().setContents(ss,ss); 
	    }
	}
	else if(event.equals("CopyBottom")) {
	    String s = botta.getSelectedText(); 
	    if(s != null) {
		StringSelection ss = new StringSelection(s); 
		this.getToolkit().getSystemClipboard().setContents(ss,ss); 
	    }
	}   
	else if(event.equals("Save...")){
	    int retValue = jfc.showSaveDialog(this);
	    if(retValue == JFileChooser.APPROVE_OPTION){
		try{
		    FileOutputStream fos = new FileOutputStream(jfc.getSelectedFile());
		    byte data[] = topta.getText().trim().getBytes();
		    fos.write(data);
		    fos.close();
		} 
		catch(Exception ex){
		    IRRj.log.append(ex.toString());
		    IRRj.optWin.statLabel.setText("Problem saving file, see log");
		    IRRj.optWin.tabbedPane.setSelectedIndex(3);
		    IRRj.optWin.setVisible(true);
		}
	    }
	}
	else if(event.equals("Load...")){
	    int retValue = jfc.showOpenDialog(this);
	    if(retValue == JFileChooser.APPROVE_OPTION){
		try {
		    FileInputStream fis = new FileInputStream(jfc.getSelectedFile());
		    byte [] data = new byte [ fis.available() ];
		    fis.read(data);
		    fis.close();
		    topta.setText(new String(data) + IRRj.newLine);
		}
		catch (Exception ex ) {
		    IRRj.log.append(ex.toString());
		    IRRj.optWin.statLabel.setText("Problem loading file, see log");
		    IRRj.optWin.tabbedPane.setSelectedIndex(3);
		    IRRj.optWin.setVisible(true);
		}
	    }
	}
	else
	    new ObjectWizard(topta);
    }
    
    public String handlePgp(boolean rps, String buf){
	String text = new String();
	Process pgp = null;
	String temp_uid =  IRRj.optWin.options.pus;
	String temp_exec = IRRj.optWin.options.pes;
	String temp_pwd = IRRj.optWin.options.pps;
	String temp_path = IRRj.optWin.options.prs;
	DataInputStream s_err = null;
	DataInputStream s_in = null;
	PrintWriter s_out = null;
	
	String EXEC_STRING = null;
	if(temp_uid.equals("") || temp_exec.equals("") || temp_pwd.equals("") || temp_path.equals("")){
	    IRRj.optWin.statLabel.setText("Make sure all PGP attributes are filled");
	    IRRj.optWin.tabbedPane.setSelectedIndex(2);
	    IRRj.optWin.setVisible(true);
	    return null;
	}
	
	try{
	    /* PGP v5 */
	    if(temp_exec.endsWith("pgps"))
		if(!rps)
		    EXEC_STRING = new String(temp_exec + " -u " + temp_uid + " -fta");
		else
		    EXEC_STRING = new String(temp_exec + " -u " + temp_uid + " -ftab");
	    /* assume pgp 2.6 */
	    else
		if(!rps)
		    EXEC_STRING = new String(temp_exec + " -fsta -u " + temp_uid);
		else
		    EXEC_STRING = new String(temp_exec + " -fstab -u " + temp_uid);

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
	    s_out.print(buf);
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
				IRRj.optWin.statLabel.setText("Problem with keyrings, check PGP path");
				IRRj.optWin.tabbedPane.setSelectedIndex(2);
				IRRj.optWin.setVisible(true);
				s_in.close();
				s_err.close();
				pgp.destroy();
				return null;
			    }
			}
			IRRj.optWin.statLabel.setText("Invalid user ID (PGP=" + pgp.exitValue() + ")");
			IRRj.optWin.tabbedPane.setSelectedIndex(2);
			IRRj.optWin.setVisible(true);
			s_in.close();
			s_err.close();
			pgp.destroy();
			return null;
		    }
		}
		catch( IllegalThreadStateException e ){
		    if(numTimes > 10){
			IRRj.optWin.statLabel.setText("Check PGP Password\n");
			IRRj.optWin.tabbedPane.setSelectedIndex(2);
			IRRj.optWin.setVisible(true);
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
	    IRRj.optWin.statLabel.setText("Could not find " + temp_exec);
	    IRRj.optWin.tabbedPane.setSelectedIndex(2);
	    IRRj.optWin.setVisible(true);
	    return null;
	}
	catch(SecurityException e){
	    IRRj.log.append("Security exception when calling external pgp\n");
	}
	/* if the pgp process is running, it shouldn't be, kill it */
	/* if some other error occurred, make user aware */
	try{
	    if( pgp.exitValue() != 0 ){
		byte err_msg[] = new byte[s_err.available()];
		s_err.read(err_msg);
		IRRj.optWin.statLabel.setText(new String(err_msg));
		IRRj.optWin.tabbedPane.setSelectedIndex(2);
		IRRj.optWin.setVisible(true);
		pgp.destroy();
		return null;
	    }
	}
	catch( IllegalThreadStateException e ){
	    IRRj.log.append("killed external proc\n");  
	    pgp.destroy();
	}
	catch( IOException e){}
	
	return text;
    }  

    public String timeStamp(){
	SimpleDateFormat formatter = new SimpleDateFormat ("yyyyMMdd hh:mm:ss +00:00");
	Date currentTime = new Date();
	String dateString = formatter.format(currentTime);
	return ("timestamp: " + dateString );
    }
}

