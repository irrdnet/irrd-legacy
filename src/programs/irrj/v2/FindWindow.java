import java.awt.event.*;
import java.awt.*;
import javax.swing.*;
import java.util.*;
import javax.swing.text.*;
import java.awt.BorderLayout;
import java.awt.Color;
import java.net.*;
import java.io.*;

public class FindWindow extends JFrame implements ActionListener{
    private String lastSearch;
    private JTextField ftf = new JTextField(20);
    
    public FindWindow(){
	super("Find");
	Container jp = getContentPane();
	jp.setLayout(new BoxLayout(jp, BoxLayout.Y_AXIS));
	jp.add(ftf);
	
	JPanel buttonPanel = new JPanel();
	
	JButton findButton = new JButton("Find");
	findButton.addActionListener(this);
	
	JButton cancelButton = new JButton("Cancel");
	cancelButton.addActionListener(this);
	
	buttonPanel.add(findButton);
	buttonPanel.add(cancelButton);

	jp.add(buttonPanel);
	pack();
    }

    public void actionPerformed(ActionEvent e){
	String event = e.getActionCommand();
	if(event.equals("Find")){
	    String inputValue = ftf.getText().trim();
	    String doc = IRRj.ita.getText();
	    int start = doc.indexOf(inputValue, IRRj.ita.getCaretPosition());
	    if(start >= 0)
		IRRj.ita.select(start, (start + inputValue.length()));
	    else {
		int i = JOptionPane.showConfirmDialog(null, "Not found.  Start from beginning?",
						      "choose one", JOptionPane.YES_NO_OPTION);
		if( i == JOptionPane.YES_OPTION )
		    IRRj.ita.setCaretPosition(0);
	    }
	}
	else if(event.equals("Cancel"))
	    setVisible(false);
    }
}
	
