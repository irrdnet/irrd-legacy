import java.awt.*;
import java.awt.event.*;
import java.util.*;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;

public class CheckBoxList extends JList {
    
    public CheckBoxList(Vector data) {
	super(data);
	CheckListCellRenderer renderer = new CheckListCellRenderer();
	setCellRenderer(renderer);
	setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
	CheckListener lst = new CheckListener(this);
	addMouseListener(lst);
    }
    
    void sortList() { 
	ListModel model = getModel(); 
	int numItems = model.getSize(); 
	Vector a = new Vector();
	for (int i=0;i<numItems;i++) 
	    { 
		if ( ((JCheckBox)model.getElementAt(i)).isSelected() )
		    a.add((JCheckBox)model.getElementAt(i));
	    } 
	for (int i=0;i<numItems;i++) 
	    { 
		if ( !((JCheckBox)model.getElementAt(i)).isSelected())
		    a.add((JCheckBox)model.getElementAt(i));
	    } 
	setListData(a); 
	revalidate(); 
    } 
}

class CheckListCellRenderer extends JCheckBox implements ListCellRenderer {
    protected static Border m_noFocusBorder = new EmptyBorder(1, 1, 1, 1);
    public CheckListCellRenderer() {
	super();
    }
    
    public Component getListCellRendererComponent(JList list,
						  Object value, int index, boolean isSelected, boolean cellHasFocus)
    {
	JCheckBox jcb = (JCheckBox)value;
	jcb.setBackground(isSelected ? list.getSelectionBackground() : 
		      list.getBackground());
	jcb.setForeground(jcb.isSelected() ? Color.blue : Color.red);
	jcb.setFont(list.getFont());
	jcb.setBorder((cellHasFocus) ? 
		      UIManager.getBorder("List.focusCellHighlightBorder")
		      : m_noFocusBorder);
	return jcb;
    }
}

class CheckListener implements MouseListener{
    protected CheckBoxList m_parent;

    public CheckListener(CheckBoxList parent) {	
	m_parent = parent;
    }
    
    public void mouseClicked(MouseEvent e) {
	if(e.getX() < 20){
	    int index = m_parent.getSelectedIndex();	
	    if (index < 0)
		return;
	    JCheckBox jcb = (JCheckBox)m_parent.getSelectedValue();
	    if(jcb.isSelected())
		jcb.setSelected(false);
	    else
		jcb.setSelected(true);
	    m_parent.repaint();
	    m_parent.sortList();
	}
	else{
	    String dbName = ((JCheckBox)m_parent.getSelectedValue()).getText();
	    IRRj.optWin.serverLabel.setText( IRRj.gr.getSockInfo() );
	    IRRj.optWin.dbLabel.setText( dbName );
	    IRRj.gr.getNewRequest("!j"+dbName+"\n");
	    if(IRRj.gr.getBuffer() != null){
		StringTokenizer strtok = new StringTokenizer(IRRj.gr.getBuffer(), ":-");
		strtok.nextToken();
		strtok.nextToken();
		IRRj.optWin.startSerial.setText( strtok.nextToken().trim() );
		IRRj.optWin.endSerial.setText( strtok.nextToken().trim() );
	    }
	    else{
		IRRj.optWin.startSerial.setText( "<error>" );
		IRRj.optWin.endSerial.setText( "<error>" );
	    }
	}
    }
    
    public void mousePressed(MouseEvent e) {}
    
    public void mouseReleased(MouseEvent e) {}
    
    public void mouseEntered(MouseEvent e) {}
    
    public void mouseExited(MouseEvent e) {}
}
