import java.awt.event.*;
import java.awt.*;
import javax.swing.*;
import java.util.*;
import javax.swing.text.*;
import java.awt.BorderLayout;
import java.awt.Color;
import java.net.*;
import java.io.*;

public class IRRj extends JFrame implements ActionListener{
    public static IrrdTextArea ita = new IrrdTextArea(20, 40);
    public static JTextArea log = new JTextArea(12, 30);
    public static GetRequest gr;
    IrrdSearchPane sPane = new IrrdSearchPane(this);
    JButton backButton;
    JButton forButton;
    Font smallFont = new Font("Monospaced", Font.PLAIN, 11);
    protected JButton stopButton;
    public static OptionsWindow optWin;
    public static JProgressBar jpb = new JProgressBar(0, 100);
    public static Properties prop = new Properties();
    public final static String newLine = "\n";
    public static Hashtable iconHash = new Hashtable();
    public FindWindow finder = new FindWindow();

    public IRRj(){
	super("IRRj 2.0");
	addWindowListener(new WindowAdapter(){
		public void windowClosing(WindowEvent e){
		    System.exit(1);
		} 
	    });
	buildLayout();
	sPane.input.addKeyListener (new KeyAdapter () {
		public void keyPressed (KeyEvent e) {
		    if (e.getKeyCode() == KeyEvent.VK_ENTER) 
			handleSubmit();
		}
	    });
	loadIcons();
	loadDefaults();
	ita.setSearchPane(sPane);
	ita.setSelectionColor(Color.red);
	log.setEditable(false);
	jpb.setStringPainted(true);
	jpb.setString("Ready");
	pack();
	setVisible(true);
    }
    
    public static void main(String args[]){
	Thread t = new Thread(){
		public void run(){
		    new IRRj();
		}
	    };
	t.start();
    }
    
    public static void loadDefaults(){
	try{
	    FileInputStream in = new FileInputStream("defaults.irr");
	    prop.load(in);
	    in.close();
	}
	catch(Exception e){}
	String server = prop.getProperty("queryserver", "whois.radb.net");
	int port = Integer.valueOf(prop.getProperty("queryport", "43")).intValue();
	gr = new GetRequest(server, port, GetRequest.STAY_CONNECTED);
	optWin = new OptionsWindow();
    }
    
    public void loadIcons(){
	iconHash.put("undoIcon", loadJarImage("Undo24.gif"));
	iconHash.put("redoIcon",  loadJarImage("Redo24.gif"));
	iconHash.put("copyIcon", loadJarImage("Copy24.gif"));
	iconHash.put("pasteIcon", loadJarImage("Paste24.gif"));
	iconHash.put("saveIcon", loadJarImage("Save24.gif"));
	iconHash.put("loadIcon", loadJarImage("Open24.gif"));
	iconHash.put("copyIconSmall", loadJarImage("Copy16.gif"));
	iconHash.put("clearIcon", loadJarImage("Delete16.gif"));
    }

    public void buildLayout(){
	buildMenuBar();
	buildToolBar();
	JButton qButton = new JButton("Submit");
	qButton.setFont(smallFont);
	qButton.addActionListener(this);
	sPane.add(qButton, BorderLayout.EAST);
	JPanel centerPane = new JPanel();
	centerPane.setLayout(new BorderLayout());
	centerPane.add(sPane, BorderLayout.NORTH);
	JScrollPane jsp = new JScrollPane(ita);
	centerPane.add(jsp, BorderLayout.CENTER);
	getContentPane().add(centerPane, BorderLayout.CENTER);
	getContentPane().add(jpb, BorderLayout.SOUTH);
    }

    public void buildMenuBar(){
	JMenuBar topMenuBar = new JMenuBar();
	setJMenuBar(topMenuBar);
	JMenu fileMenu = new JMenu("File");
	topMenuBar.add(fileMenu);
	JMenu editMenu = new JMenu("Edit");
	topMenuBar.add(editMenu);
	
	JMenuItem newMenuItem = new JMenuItem("New Object");
	fileMenu.add(newMenuItem);
	newMenuItem.addActionListener(this);
	JMenuItem exitMenuItem = new JMenuItem("Exit");
	fileMenu.addSeparator();
	fileMenu.add(exitMenuItem);
	exitMenuItem.addActionListener(this);

	JMenuItem copyMenuItem = new JMenuItem("Copy");
	editMenu.add(copyMenuItem);
	copyMenuItem.addActionListener(this);
	JMenuItem prefMenuItem = new JMenuItem("Preferences...");
	prefMenuItem.setActionCommand("Pref");
	editMenu.addSeparator();
	editMenu.add(prefMenuItem);
	prefMenuItem.addActionListener(this);
    }

    public void buildToolBar(){
	JToolBar topToolBar = new JToolBar();
	topToolBar.setFloatable(false);

	ImageIcon backIcon = loadJarImage("Back24.gif");
	backButton = new JButton(backIcon);
	backButton.setEnabled(false);
	backButton.setToolTipText("Back");
	backButton.setActionCommand("Back");
	backButton.addActionListener(this);	
	
	ImageIcon forIcon = loadJarImage("Forward24.gif");
	forButton = new JButton(forIcon);
	forButton.setEnabled(false);
	forButton.setToolTipText("Forward");
	forButton.setActionCommand("Forward");
	forButton.addActionListener(this);

	ImageIcon findIcon = loadJarImage("Find24.gif");
	JButton findButton = new JButton(findIcon);
	findButton.setToolTipText("Find");
	findButton.setActionCommand("Find");
	findButton.addActionListener(this);

	ImageIcon prefIcon = loadJarImage("Preferences24.gif");
	JButton prefButton = new JButton(prefIcon);
	prefButton.setToolTipText("Preferences");
	prefButton.setActionCommand("Pref");
	prefButton.addActionListener(this);

	ImageIcon stopIcon = loadJarImage("Stop24.gif");
	stopButton = new JButton(stopIcon);
	stopButton.setEnabled(false);
	stopButton.setToolTipText("Stop");
	stopButton.setActionCommand("Stop");
	stopButton.addActionListener(this);

	topToolBar.add(backButton);
	topToolBar.add(forButton);
	topToolBar.add(findButton);
	topToolBar.add(prefButton);
	topToolBar.add(stopButton);

	getContentPane().add(topToolBar, BorderLayout.NORTH);
    }

    public ImageIcon loadJarImage(String name){
	URL url = getClass().getResource("images/" + name); 
	if (url != null) { 
	    Image image = Toolkit.getDefaultToolkit().getImage(url); 
	    if (image != null) { 
		return new ImageIcon(image); 
	    } 
	}
	return null;
    } 
	
    public synchronized void handleSubmit(){
	if(sPane.getRequest() == null)
	    return;
	Cursor curs = new Cursor (Cursor.WAIT_CURSOR);
	setCursor (curs);
	stopButton.setEnabled(true);
	backButton.setEnabled(false);

	jpb.setString("Working...");
	
	Thread s = new Thread(){
		public void run(){
		    gr.getNewRequest(sPane.getRequest(), ita, jpb);
		    sPane.handleSubmit(ita.getText());
		    setCursor (Cursor.getDefaultCursor());
		    jpb.setString("Ready");
		    stopButton.setEnabled(false);
		    backButton.setEnabled(true);
		    jpb.setValue(0);
		}
	    };
	s.start();
    }

    public void actionPerformed(ActionEvent e){
	String event = e.getActionCommand();
	if(event.equals("Submit"))
	    handleSubmit();
	else if(event.equals("Exit")){
	    System.exit(1);
	}
	else if(event.equals("Back")){
	    String doc = sPane.back();
	    if(doc != null){
		ita.setText(doc);
		forButton.setEnabled(true);
	    }
	    else
		backButton.setEnabled(false);
	}
	else if(event.equals("Forward")){
	    String doc = sPane.forward();
	    if(doc != null){
		ita.setText(doc);
		backButton.setEnabled(true);
	    }
	    else
		forButton.setEnabled(false);
	}
	else if(event.equals("Copy"))
	    ita.copy();
	else if(event.equals("Stop")){
	    log.append("Action Stopped\n");
	    gr.stopQuery();
	}
	else if(event.equals("Find"))
	    finder.setVisible(true);
	else if(event.equals("New Object"))
	    new EditorWindow();
	else if(event.equals("Pref"))
	    optWin.setVisible(true);
    }
}

