import java.awt.event.*;
import java.awt.*;
import javax.swing.*;
import java.util.*;
import javax.swing.text.*;
import java.awt.BorderLayout;
import java.awt.Color;
import java.net.*;
import java.io.*;
import javax.swing.border.*;

public class ObjectWizard extends JFrame implements ActionListener, ItemListener{
    public Dimension defaultSize = new Dimension(250, 250);
    JTextArea parent;
    Font smallFont = new Font("Monospaced", Font.PLAIN, 12);
    JComboBox objs = new JComboBox();
    JScrollPane cur = new JScrollPane();
    JTextArea leftTextArea = new JTextArea(10, 10);
    Vector first, curVector;
    Vector[] objVectors = new Vector[5];
    Vector commonVector = new Vector();
    JLabel topLabel =  new JLabel("Type");
    JLabel requiredLabel = new JLabel("Mandatory");
    JLabel occuranceLabel = new JLabel("Single");
    int curInternalIndex = -1;
    int curVectorIndex = 0; /* 0 = type, 1 = selected type, 2 = common*/
    JButton backButton;
    JButton nextButton;
    JButton finishButton;
    RpslObject typeObj = new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE, 
					"Type", "Choose type of object", buildTypePanel());
    Font italicFont = new Font("Monospaced", Font.ITALIC, 11);

    /* common components */
    JTextArea descrtf = new JTextArea(5, 5);
    JTextField techctf = new JTextField();
    JTextArea adminctf = new JTextArea(5, 5);
    JTextArea remarkstf = new JTextArea(5, 5);
    JTextArea notifytf = new JTextArea(5, 5);
    JTextArea mntbytf = new JTextArea(5, 5);
    JTextArea changedtf = new JTextArea(5, 5);
    JTextField sourcetf = new JTextField();

    /* maint */
    JTextField mainttf = new JTextField();
    JTextArea authtf = new JTextArea(5, 5);

    /* person */
    JTextField persontf = new JTextField();
    JTextField nichdltf = new JTextField();
    JTextArea addresstf = new JTextArea(5, 5);
    JTextArea phonetf = new JTextArea(5, 5);
    JTextArea faxtf = new JTextArea(5, 5);
    JTextArea emailtf = new JTextArea(5, 5);

    /* role */
    JTextField roletf = new JTextField();
    JTextField troubletf = new JTextField();

    /* autnum */
    JTextField autnumtf = new JTextField();
    JTextField asnametf = new JTextField();
    JTextArea memberoftf = new JTextArea(5, 5);
    JTextArea importtf = new JTextArea(5, 5);
    JTextArea exporttf = new JTextArea(5, 5);
    JTextArea defaulttf = new JTextArea(5, 5);

    /* route */
    JTextField routetf = new JTextField();
    JTextField origintf = new JTextField();
    JTextArea injecttf = new JTextArea(5, 5);
    JTextArea componentstf = new JTextArea(5, 5);
    JTextArea aggrbndrytf = new JTextArea(5, 5);
    JTextArea aggrmtdtf = new JTextArea(5, 5);
    JTextArea exportcompstf = new JTextArea(5, 5);
    JTextArea holestf = new JTextArea(5, 5);

    public ObjectWizard(JTextArea parentIn){
	super("Object Creation Wizard");
	leftTextArea.setEditable(false);
	leftTextArea.setBackground(Color.lightGray);
	topLabel.setForeground(Color.black);
	occuranceLabel.setFont(italicFont);
	requiredLabel.setFont(italicFont);
	parent = parentIn;
	buildLayout();
	pack();
	show();
    }
    
    private void buildLayout() {
	JPanel buttonPanel = new JPanel();
	backButton = new JButton("Back");
	backButton.addActionListener(this);
	backButton.setEnabled(false);
	nextButton = new JButton("Next");
	nextButton.addActionListener(this);
	JButton cancelButton = new JButton("Cancel");
	cancelButton.addActionListener(this);
	finishButton = new JButton("Finish");
	finishButton.addActionListener(this);
	finishButton.setEnabled(false);
	buttonPanel.add(backButton);
	buttonPanel.add(nextButton);
	buttonPanel.add(cancelButton);
	buttonPanel.add(finishButton);

	JPanel leftPanel = new JPanel(new BorderLayout());
	leftPanel.add(requiredLabel, BorderLayout.NORTH);
	JPanel innerLeftPanel = new JPanel(new BorderLayout());
	innerLeftPanel.add(occuranceLabel, BorderLayout.NORTH);
	innerLeftPanel.add(leftTextArea);
	leftPanel.add(innerLeftPanel);

	JPanel centerPane = new JPanel(new GridLayout(1, 2));
	//centerPane.add(leftTextArea);
	centerPane.add(leftPanel);
	centerPane.add(cur);
	cur.setViewportView(typeObj.panel);
	leftTextArea.setText(typeObj.description);

	getContentPane().add(topLabel, BorderLayout.NORTH);
	getContentPane().add(centerPane);
	getContentPane().add(buttonPanel, BorderLayout.SOUTH);
	buildObjectVectors();
    }
    
    private JPanel buildTypePanel(){
	//JLabel lbl = new JLabel("Type of object?");
	//lbl.setFont(smallFont);
	objs.addItemListener(this);
	objs.addItem("Mntner");
	objs.addItem("Person");
	objs.addItem("Aut-num");
	objs.addItem("Route");
	objs.addItem("Role");

	JPanel jp = new JPanel();
	//jp.setLayout(new BoxLayout(jp, BoxLayout.Y_AXIS));
	//jp.add(lbl);
	jp.add(objs);
	return jp;
    }
			 
    private void buildObjectVectors(){
	buildCommonVector();
	objVectors[0] = buildMaintVector();
	objVectors[1] = buildPersonVector();
	objVectors[2] = buildAutnumVector();
	objVectors[3] = buildRouteVector();
	objVectors[4] = buildRoleVector();
    }
    
    private void buildCommonVector(){
	commonVector.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
					       "descr:", "A description of this object", 
					       descrtf));
	commonVector.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
					       "tech-c:", "Nic handles for the\ntechnical contacts", 
					       techctf));
	commonVector.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
					       "admin-c:", "Nic handles for the\nadministrative contacts", 
					       adminctf));
	commonVector.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
					       "remarks:", 
					       "Any text you want to be associated\nwith this object", 
					       remarkstf));
	commonVector.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
					       "notify:", 
					       "When object is updated,\nsend email to these\npeople", 
					       notifytf));
	commonVector.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
					       "mnt-by:", 
					       "Maintainer objects that\n are allowed to change\n" +
					       "this object", 
					       mntbytf));
	commonVector.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
					       "changed:", 
					       "Of the form:\n user@host.xxx YYYYMMDD ", 
					       changedtf));
	commonVector.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
					       "source:", 
					       "The name of the database\nin which the object belongs", 
					       sourcetf));
    }
    
    private Vector buildMaintVector(){
	Vector v = new Vector();
	v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				    "mntner:", "The name of the maintainer", 
				    mainttf));
	v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
				    "auth:", 
				    "Type of authorization used:\nCRYPT-PW\n"+
				    "PGP-KEY\nNONE\nMAIL-FROM", 
				    authtf));
	return v;
    }

     private Vector buildPersonVector(){
	 Vector v = new Vector();
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				     "person:", 
				     "A person's name", 
				     persontf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				     "nic-hdl:", 
				     "The person's nic handle", 
				     nichdltf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
				     "address:", 
				     "The person's postal address", 
				     addresstf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
				     "phone:", 
				     "The person's phone\n(xxx)xxx-xxxx", 
				     phonetf));
	 v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				     "fax-no:", 
				     "The person's fax\n(xxx)xxx-xxxx", 
				     faxtf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
				     "e-mail:", 
				     "The person's email", 
				     emailtf));
	 return v;
     }

    private Vector buildAutnumVector(){
	 Vector v = new Vector();
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				     "aut-num:", 
				     "An autonomous system number", 
				     autnumtf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				     "as-name:", 
				     "The symbolic name of the AS", 
				     asnametf));
	 v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				     "member-of:", 
				     "List of as-sets", 
				     memberoftf));
	 v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				     "import:", 
				     "No description", 
				     importtf));
	 v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				     "export:", 
				     "No description", 
				     exporttf));
	 v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				     "default:", 
				     "No description", 
				     defaulttf));
	 return v;
    }

    private Vector buildRouteVector(){
	Vector v = new Vector();
	v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				    "route:", 
				    "A route:\nxxx.xxx.xxx.xxx/xx", 
				    routetf));
	v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				    "origin:", 
				    "An autonomous system number that originate this route", 
				    origintf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "member-of:", 
				    "A list of route-sets", 
				    memberoftf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "inject:", 
				    "Optional\nMulti-valued\n", 
				    injecttf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "components:", 
				    "Optional\nMulti-valued\n", 
				    componentstf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "aggr-bndry:", 
				    "Optional\nMulti-valued\n", 
				    aggrbndrytf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "aggr-mtd:", 
				    "Optional\nMulti-valued\n", 
				    aggrmtdtf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "export-comps:", 
				    "Optional\nMulti-valued\n", 
				    exportcompstf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "holes:", 
				    "Optional\nMulti-valued\n", 
				    holestf));
	return v;
    }

    private Vector buildRoleVector(){
	Vector v = new Vector();
	v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				    "role:", 
				    "A person's role", 
				    roletf));
	v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.SINGLE,
				    "nic-hdl:", 
				    "The person's nic handle", 
				    nichdltf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "trouble:", 
				    "Optional\nMulti-valued\n", 
				    troubletf));
	v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				    "address:", 
				    "The person's postal address", 
				    addresstf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
				     "phone:", 
				     "The person's phone\n(xxx)xxx-xxxx", 
				     phonetf));
	 v.addElement(new RpslObject(RpslObject.OPTIONAL, RpslObject.MULTI,
				     "fax-no:", 
				     "The person's fax\n(xxx)xxx-xxxx", 
				     faxtf));
	 v.addElement(new RpslObject(RpslObject.MANDATORY, RpslObject.MULTI,
				     "e-mail:", 
				     "Mandatory\nMulti-valued\nThe person's email", 
				     emailtf));
	 return v;
    }


    public void actionPerformed(ActionEvent e){
	String event = e.getActionCommand();
	if(event.equals("Next")){
	    if(curVectorIndex != 0){
		RpslObject curObj1 = (RpslObject)curVector.get(curInternalIndex);
		if( ((JTextComponent)curObj1.panel).getText().trim().length() == 0  &&
		    curObj1.requiredPolicy == RpslObject.MANDATORY){
		    JOptionPane.showMessageDialog(null, "This is a required field", 
						  "Field Required", JOptionPane.ERROR_MESSAGE);
		    return;
		}   
	    }
	    if(curVectorIndex == 0){
		curVector = (Vector)objVectors[objs.getSelectedIndex()];
		first = curVector;
		curVectorIndex++;
	    }
	    curInternalIndex++;
	    if(curInternalIndex > -1 && curInternalIndex < curVector.size()){
		RpslObject curObj = (RpslObject)curVector.get(curInternalIndex);
		if(curObj.requiredPolicy == RpslObject.MANDATORY)
		    requiredLabel.setText("Mandatory Field");
		else
		    requiredLabel.setText("Optional Field");
		if(curObj.occurancePolicy == RpslObject.MULTI)
		    occuranceLabel.setText("Multi-valued");
		else
		    occuranceLabel.setText("Single-valued");
		topLabel.setText(curObj.label);
		leftTextArea.setText(curObj.description);
		cur.setViewportView(curObj.panel);
	    }
	    else if(curVectorIndex < 2){
		curVectorIndex++;
		curVector = commonVector;
		curInternalIndex = 0;
		RpslObject curObj = (RpslObject)curVector.get(curInternalIndex);
		if(curObj.requiredPolicy == RpslObject.MANDATORY)
		    requiredLabel.setText("Mandatory Field");
		else
		    requiredLabel.setText("Optional Field");
		if(curObj.occurancePolicy == RpslObject.MULTI)
		    occuranceLabel.setText("Multi-valued");
		else
		    occuranceLabel.setText("Single-valued");
		topLabel.setText(curObj.label);
		leftTextArea.setText(curObj.description);
		cur.setViewportView(curObj.panel);
	    }
	    else{
		nextButton.setEnabled(false);
		finishButton.setEnabled(true);
	    }
	    backButton.setEnabled(true);
	}
	else if(event.equals("Back")){
	    curInternalIndex--;
	    if(curInternalIndex < 0){
		if(curVectorIndex == 2){
		    curVector = first;
		    curInternalIndex = curVector.size()-1;
		    curVectorIndex--;
		    RpslObject curObj = (RpslObject)curVector.get(curInternalIndex);
		    if(curObj.requiredPolicy == RpslObject.MANDATORY)
			requiredLabel.setText("Mandatory Field");
		    else
			requiredLabel.setText("Optional Field");
		    if(curObj.occurancePolicy == RpslObject.MULTI)
			occuranceLabel.setText("Multi-valued");
		    else
			occuranceLabel.setText("Single-valued");
		    topLabel.setText(curObj.label);
		    leftTextArea.setText(curObj.description);
		    cur.setViewportView(curObj.panel);
		}
		else if(curVectorIndex == 1){
		    curVectorIndex--;
		    cur.setViewportView(typeObj.panel);
		    topLabel.setText(typeObj.label);
		    leftTextArea.setText(typeObj.description);
		    backButton.setEnabled(false);
		}
	    }
	    else{
		RpslObject curObj = (RpslObject)curVector.get(curInternalIndex);
		if(curObj.requiredPolicy == RpslObject.MANDATORY)
		    requiredLabel.setText("Mandatory Field");
		else
		    requiredLabel.setText("Optional Field");
		if(curObj.occurancePolicy == RpslObject.MULTI)
		    occuranceLabel.setText("Multi-valued");
		else
		    occuranceLabel.setText("Single-valued");
		topLabel.setText(curObj.label);
		leftTextArea.setText(curObj.description);
		cur.setViewportView(curObj.panel);
	    }
	    nextButton.setEnabled(true);
	    finishButton.setEnabled(false);
	}
	else if(event.equals("Cancel"))
	    dispose();
	else if(event.equals("Finish")){
	    for(int i = 0; i < first.size(); i++)
		((RpslObject)first.get(i)).dump(parent);
	    for(int i = 0; i < commonVector.size(); i++)
		((RpslObject)commonVector.get(i)).dump(parent);
	    dispose();
	}
    }

    public void itemStateChanged(ItemEvent e){
	if(e.getStateChange() == ItemEvent.SELECTED){
	    }
    }
}
