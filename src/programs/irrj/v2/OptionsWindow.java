import java.awt.event.*;
import java.awt.*;
import javax.swing.*;
import java.util.*;
import javax.swing.text.*;
import java.awt.BorderLayout;
import java.awt.Color;
import java.net.*;
import javax.swing.border.*;
import java.io.*;
import java.awt.datatransfer.*;

public class OptionsWindow extends JFrame implements ActionListener {
    final JLabel qs = new JLabel("Query Server");
    final JLabel qp = new JLabel("Query Port");
    final JLabel ss = new JLabel("Submit Server");
    final JLabel sp = new JLabel("Submit Port");
    JTextField qstf = new JTextField(20);
    LimitTextField qptf = new LimitTextField(5, 65536);
    JTextField sstf = new JTextField(20);
    LimitTextField sptf = new LimitTextField(5, 65536);

    final JLabel mpass = new JLabel("Maintainer Password");
    final JLabel ppass = new JLabel("PGP Password");
    final JLabel pguid = new JLabel("PGP User ID");
    final JLabel pgexe = new JLabel("PGP Executable");
    final JLabel pgrin = new JLabel("Full path to PGP rings");
    JTextField mpasstf = new JPasswordField(20);
    JTextField ppasstf = new JPasswordField(20);
    JTextField pguidtf = new JTextField(20);
    JTextField pgexetf = new JTextField(20);
    JTextField pgrintf = new JTextField(20);
    CheckBoxList dbList;

    private final String DB_REQUEST = "!s-lc";
    private final String DB_RESET = "!s-*";
    private final String DB_SELECT = "!s";
    OptionsData options;
    public JLabel statLabel = new JLabel();
    public JTabbedPane tabbedPane;
    JLabel serverLabel = new JLabel();
    JLabel dbLabel = new JLabel();
    JLabel startSerial = new JLabel();
    JLabel endSerial = new JLabel();

    public OptionsWindow(){
	super("IRRj Options");
	addWindowListener(new WindowAdapter(){
		public void windowClosing(WindowEvent e){
		    setVisible(false);
		    options.revert(dbList.getModel());
		    statLabel.setText("");
		} 
	    });
	buildLayout();
	statLabel.setForeground(Color.red);
	qstf.setText(IRRj.prop.getProperty("queryserver", "whois.radb.net"));
	qptf.setText(IRRj.prop.getProperty("queryport", "43"));
	sstf.setText(IRRj.prop.getProperty("submitserver", "whois.radb.net"));
	sptf.setText(IRRj.prop.getProperty("submitport", "8888"));
	pguidtf.setText(IRRj.prop.getProperty("pgpuid"));
	pgexetf.setText(IRRj.prop.getProperty("pgpexe"));
	pgrintf.setText(IRRj.prop.getProperty("pgpring"));
	options = new OptionsData(qstf, qptf, sstf, sptf, mpasstf, ppasstf, 
				  pguidtf, pgexetf, pgrintf, dbList.getModel());
	pack();
    }

    public void buildLayout(){
	tabbedPane = new JTabbedPane();
	
	JPanel serverPanel = buildServerPanel();
	JPanel databasePanel = buildDatabasePanel();
	JPanel logPanel = buildLogPanel();
	logPanel.setPreferredSize(serverPanel.getSize());
	JPanel authPanel = buildAuthPanel();
	JScrollPane aboutPanel = buildAboutPanel();

	tabbedPane.addTab("Servers", null, serverPanel, "Change Query and Submit servers");
	tabbedPane.addTab("Databases", null, databasePanel, "Change which databases to query");
	tabbedPane.addTab("Authorization", null, authPanel, "Change Maintainer password and PGP options");
	tabbedPane.addTab("Log", null, logPanel, "View Message log");
	tabbedPane.addTab("About", null, aboutPanel, "About IRRj 2.0");
	getContentPane().add(tabbedPane);
	
	JPanel buttonPane = new JPanel();
	JButton okButton = new JButton("Ok");
	okButton.addActionListener(this);
	JButton cancelButton = new JButton("Cancel");
	cancelButton.addActionListener(this);
	JButton saveButton = new JButton("Save");
	saveButton.addActionListener(this);
	buttonPane.add(okButton);
	buttonPane.add(cancelButton);
	buttonPane.add(saveButton);

	JPanel bottomPane = new JPanel();
	bottomPane.setLayout(new BoxLayout(bottomPane, BoxLayout.Y_AXIS));
	//bottomPane.setLayout(new GridLayout(2, 1));
	bottomPane.add(statLabel);
	bottomPane.add(buttonPane);
	
	getContentPane().add(bottomPane, BorderLayout.SOUTH);
    }

    public JPanel buildServerPanel(){
	JPanel sPanel = new JPanel();
	sPanel.setLayout(new GridLayout(0,2));
	sPanel.add(qs);
	sPanel.add(qstf);
	sPanel.add(qp);
	sPanel.add(qptf);
	sPanel.add(ss);
	sPanel.add(sstf);
	sPanel.add(sp);
	sPanel.add(sptf);
	
	return sPanel;
    }

    public JPanel buildAuthPanel(){
	JPanel dPanel = new JPanel();
	dPanel.setLayout(new GridLayout(0,2));
	dPanel.add(mpass);
	dPanel.add(mpasstf);
	dPanel.add(ppass);
	dPanel.add(ppasstf);
	dPanel.add(pguid);
	dPanel.add(pguidtf);
	dPanel.add(pgexe);
	dPanel.add(pgexetf);
	dPanel.add(pgrin);
	dPanel.add(pgrintf);
	
	return dPanel;
    }
    
    public JPanel buildLogPanel(){
	JPanel lPanel = new JPanel();
	lPanel.setLayout(new BorderLayout());
	JToolBar bar = new JToolBar(JToolBar.VERTICAL);
	bar.setFloatable(false);
	
	JButton copyButton = new JButton((ImageIcon)IRRj.iconHash.get("copyIconSmall"));
	copyButton.setToolTipText("Copy");
	copyButton.setActionCommand("Copy");
	copyButton.addActionListener(this);	

	JButton clearButton = new JButton((ImageIcon)IRRj.iconHash.get("clearIcon"));
	clearButton.setToolTipText("Clear Log");
	clearButton.setActionCommand("Clear");
	clearButton.addActionListener(this);	

	bar.add(copyButton);
	bar.add(clearButton);
	
	lPanel.add(bar, BorderLayout.WEST);
	lPanel.add(new JScrollPane(IRRj.log));
	return lPanel;
    }
    
    public JScrollPane buildAboutPanel(){
	JTextPane jtp = new JTextPane();
	JScrollPane jsp = new JScrollPane(jtp);
	jtp.replaceSelection("polandj@merit.edu\n");
	jtp.replaceSelection("\nIRRj 2.0\nWritten by Jonathan Poland\nPart of the IRRd/RADB project\n");
	jtp.insertIcon(new ImageIcon("images/irrj.gif"));
	jtp.setEditable(false);
	return jsp;
    }

    public JPanel buildDatabasePanel(){
	dbList = new CheckBoxList(buildDbList());
	dbList.setVisibleRowCount(5);
	JScrollPane cbs = new JScrollPane(dbList);

	JPanel aPanel = new JPanel(new BorderLayout());
	aPanel.add(cbs, BorderLayout.WEST);

	JPanel rightPanel = new JPanel(new GridLayout(0, 2));
	rightPanel.add(new JLabel("Current server:"));
	rightPanel.add(serverLabel);
	rightPanel.add(new JLabel("Database:"));
	rightPanel.add(dbLabel);
	rightPanel.add(new JLabel("First serial:"));
	rightPanel.add(startSerial);
	rightPanel.add(new JLabel("Last serial:"));
	rightPanel.add(endSerial);
	aPanel.add(rightPanel);

	return aPanel;
    }

    public Vector buildDbList(){
	Vector vec = new Vector();
	int bytes = IRRj.gr.getNewRequest(DB_REQUEST);
	if (bytes < 1){
	    vec.add(new JCheckBox("Error reading from server"));
	}
	String s = IRRj.gr.getBuffer();
	if (s == null) {
	    vec.add(new JCheckBox("No Databases available"));
	}
	else{
	    StringTokenizer strtok = new StringTokenizer(s, ",");
	    while(strtok.hasMoreTokens()){
		JCheckBox cb = new JCheckBox(strtok.nextToken().trim(), true);
		vec.add(cb);
	    }
	}
	
	return vec;
    }

    public void saveDefaults(){
	Properties prop = new Properties();
	if(qstf.getText().trim() != null)
	    prop.setProperty("queryserver", qstf.getText().trim());
	if(qptf.getText().trim() != null)
	    prop.setProperty("queryport", qptf.getText().trim());
	if(sstf.getText().trim() != null)
	    prop.setProperty("submitserver", sstf.getText().trim());
	if(sptf.getText().trim() != null)
	    prop.setProperty("submitport", sptf.getText().trim());
	if(pguidtf.getText().trim() != null)
	    prop.setProperty("pgpuid", pguidtf.getText().trim());
	if(pgexetf.getText().trim() != null)
	    prop.setProperty("pgpexe", pgexetf.getText().trim());
	if(pgrintf.getText().trim() != null)
	    prop.setProperty("pgpring", pgrintf.getText().trim());
	try{
	    FileOutputStream out = new FileOutputStream("defaults.irr");
	    prop.store(out, null);
	    out.close();
	}
	catch(Exception e){
	    IRRj.log.append(e.toString());
	    statLabel.setText("Problem saving file, check log");
	}
    }

    public void actionPerformed(ActionEvent e){
	String event = e.getActionCommand();
	if(event.equals("Ok")){
	    Cursor curs = new Cursor (Cursor.WAIT_CURSOR);
	    setCursor (curs);
	    if( options.save(dbList.getModel()) ){ // if a save means a server change...update dbList;
		dbList.setListData(buildDbList());
		options.save(dbList.getModel());
	    }
	    setVisible(false);
	    statLabel.setText("");
	    setCursor (Cursor.getDefaultCursor());
	}
	else if(event.equals("Cancel")){
	    options.revert(dbList.getModel());
	    dbList.repaint();
	    setVisible(false);
	    statLabel.setText("");
	}
	else if(event.equals("Save"))
	    saveDefaults();
	else if(event.equals("Clear"))
	    IRRj.log.setText("");
	else if(event.equals("Copy")){
	    String s = IRRj.log.getSelectedText(); 
	    if(s != null) {
		StringSelection ss = new StringSelection(s); 
		this.getToolkit().getSystemClipboard().setContents(ss,ss); 
	    }
	}
    }
}
