import javax.swing.*;
import java.util.*;
import java.awt.*;
import javax.swing.text.*;
import java.awt.event.*;

public class IrrdSearchPane extends JPanel implements ItemListener{
    
    Vector savedStates = new Vector();
    int currentState = -1;
    public JTextField input = new JTextField(12);
    private JComboBox jcb = new JComboBox();
    final String[] specStrings = { "Less Specific", "Specific", "More Specific" };
    private JComboBox specBox = new JComboBox(specStrings);
    Font smallFont = new Font("Monospaced", Font.PLAIN, 11);
    private IRRj irrj;
    static final String STRING_RS = new String("Route Searches");
    static final String STRING_GA = new String("Get aut-num object");
    static final String STRING_GR = new String("Get Route with Origin");
    static final String STRING_GM = new String("Get Maintainer");
    static final String STRING_GC = new String("Get Routes with Community");
    static final String STRING_CM = new String("Send Command");

    public IrrdSearchPane(IRRj irrjIn){
	setLayout(new BorderLayout());
	jcb.addItem(STRING_RS);
	jcb.addItem(STRING_GA);
	jcb.addItem(STRING_GR);
	jcb.addItem(STRING_GM);
	jcb.addItem(STRING_GC);
	jcb.addItem(STRING_CM);
	add(jcb, BorderLayout.WEST);
	jcb.addItemListener(this);
	add(input, BorderLayout.CENTER);
	jcb.setFont(smallFont);
	specBox.setFont(smallFont);
	irrj = irrjIn;
    }

    public String getRequest(){
	String query = input.getText().trim();
	
	if(query.length() == 0)
	    return null;
	
	query = query.toUpperCase();

	if (specBox.isEnabled()){
	    String isSpecific = (String)specBox.getSelectedItem();
	    
	    if(isSpecific.equals("Less Specific")){
		if (query.indexOf("/") == -1)
		    query += "/32,l";
		else if (query.indexOf(",") == -1)
		    query += ",l";
	    }
	    else if (isSpecific.equals("More Specific")){
		if (query.indexOf("/") == -1)
		    query += "/32,M";
		else if (query.indexOf(",") == -1)
		    query += ",M";
	    }
	    return ("!r" + query);
	}
	else{
	    String choice_type = (String)jcb.getSelectedItem();
	    String ret;
	    if(choice_type.equals(STRING_GM)){
		if(query.startsWith("MAINT-"))
		    ret = new String("!mmt," + query);
		else
		    ret = new String("!mmt,MAINT-" + query);
	    }
	    else if(choice_type.equals (STRING_GA)){
		if(query.startsWith("AS"))
		    ret = new String("!man," + query);
		else
		    ret = new String("!man,AS" + query);
	    }
	    else if (choice_type.equals (STRING_GR)){
		if(query.startsWith("AS"))
		    ret = new String("!g" + query);
		else
		    ret = new String("!gAS" + query);
	    }
	    else if (choice_type.equals (STRING_GC))
		ret = new String("!h" + query);
	    else
		ret = new String("!r" + query);
	    
	    return ret;
	    }
    }
    
    public void setState(String request, String choice){
	input.setText(request);
	jcb.setSelectedItem(choice);
	irrj.handleSubmit();
    }


    public void handleSubmit(String inDoc){
	String newSearch = input.getText().trim();
	if(newSearch != null){
	    if(savedStates.size() != currentState+1)
	    	savedStates.setSize(currentState+1);
	    savedStates.addElement(new SavedState(input.getText().trim(), jcb.getSelectedIndex(), 
						  specBox.getSelectedIndex(), inDoc));
	    currentState++;
	}
    }

    public String back(){
	if(currentState < 1)
	    return null;
	
	SavedState t = (SavedState)savedStates.elementAt(--currentState);
	input.setText(t.inputText);
	jcb.setSelectedIndex(t.irrdBoxIndex);
	specBox.setSelectedIndex(t.specBoxIndex);
	return t.doc;
    }

    public String forward(){
	if(currentState+1 >= savedStates.size())
	    return null;
	
	SavedState t = (SavedState)savedStates.elementAt(++currentState);
	input.setText(t.inputText);
	jcb.setSelectedIndex(t.irrdBoxIndex);
	specBox.setSelectedIndex(t.specBoxIndex);
	return t.doc;
    }

    public void itemStateChanged (ItemEvent e) {
	String str = (String)((JComboBox)e.getSource()).getSelectedItem();
	if ( str.equals(STRING_RS) )
	    specBox.setEnabled(true);
	else
	    specBox.setEnabled(false);
    }

    protected class SavedState{
	public String inputText;
	public int irrdBoxIndex;
	public int specBoxIndex;
	public String doc;

	public SavedState(String queryText, int bIndex, int sIndex, String inDoc){
	    inputText = queryText;
	    irrdBoxIndex = bIndex;
	    specBoxIndex = sIndex;
	    doc = inDoc;
	}
    }
}
	
