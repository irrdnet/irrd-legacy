import java.awt.event.*;
import javax.swing.*;
import java.util.*;
import javax.swing.text.*;
import java.awt.*;
import java.awt.datatransfer.*;

public class IrrdTextArea extends JTextArea implements MouseListener, ActionListener{
    JPopupMenu jpm = new JPopupMenu();
    private String lastObject;
    private boolean displayingRoutes = false;
    private IrrdSearchPane isp = null;
    Font smallFont = new Font("Monospaced", Font.PLAIN, 11);

    public IrrdTextArea(){
	super();
	addMouseListener(this);
	JMenuItem modifyItem = new JMenuItem("Modify Object");
	modifyItem.addActionListener(this);
	jpm.add(modifyItem);
	JMenuItem deleteItem = new JMenuItem("Delete Object");
	deleteItem.addActionListener(this);
	jpm.add(deleteItem);
	setFont(smallFont);
	setUI(new IrrdTextAreaUI());
	setEditable(false);
    }

    public IrrdTextArea(int rows, int cols){
	super(rows, cols);
	addMouseListener(this);
	JMenuItem modifyItem = new JMenuItem("Modify Object");
	modifyItem.addActionListener(this);
	jpm.add(modifyItem);
	JMenuItem deleteItem = new JMenuItem("Delete Object");
	deleteItem.addActionListener(this);
	jpm.add(deleteItem);
	setFont(smallFont);
	setUI(new IrrdTextAreaUI());
	setEditable(false);
    }	

    public void mouseClicked(MouseEvent e){
	IRRj.gr.stopQuery();
	if (e.getModifiers() == e.BUTTON1_MASK){ 
	    PlainDocument pDoc = (PlainDocument)getDocument();
	    Element elem = pDoc.getParagraphElement(getCaretPosition());
	    try{
		String str = getText(elem.getStartOffset(), (elem.getEndOffset() - elem.getStartOffset())); 
		if(str.startsWith("route:") ){
		    isp.setState(str.substring(7).trim(), IrrdSearchPane.STRING_RS);
		} else if(str.startsWith("origin:")){
		    isp.setState(str.substring(7).trim(), IrrdSearchPane.STRING_GR);
		} else if(str.startsWith("mnt-by:")){
		    isp.setState(str.substring(7).trim(), IrrdSearchPane.STRING_GM);
		} else if( Character.isDigit(str.charAt(0))){
		    isp.setState(str.trim(), IrrdSearchPane.STRING_RS);
		    displayingRoutes = true;
		}
	    }
	    catch(Exception de){}
	}
	if(displayingRoutes)
	    return;
	
	if(e.getModifiers() == e.BUTTON3_MASK ||
	   e.getModifiers() == e.BUTTON2_MASK ){
	    int i = getUI().viewToModel( this, new Point(e.getX(), e.getY()) );
	    try{lastObject = getObjectAt(i);} catch(Exception ex){}
	    if(lastObject != null)
		jpm.show(this, e.getX(), e.getY());
	}
    }

    public void setSearchPane(IrrdSearchPane sPane){
	isp = sPane;
    }

    public void actionPerformed(ActionEvent e){
	String str = e.getActionCommand();
	if(str.equals("Modify Object"))
	    new EditorWindow(lastObject);
	else if(str.equals("Delete Object"))
	    new EditorWindow(lastObject + "delete: TCP deletion\n");
    }

    public void setText(String text){
	try{
	    if(Character.isDigit(text.charAt(0)) || text.charAt(0) == ' '){
		text = text.replace(' ', '\n');
		displayingRoutes = true;
	    }
	    else
		displayingRoutes = false;
	    super.setText(text);
	}
	catch(Exception e){
	    super.setText(text);
	}
    }

    public boolean displayingRoutes(){
	return displayingRoutes;
    }

    public void displayingRoutes(boolean value){
	displayingRoutes = value;
    }
    
    public void copy(){
	String s = getSelectedText(); 
	if(s != null){
	    StringSelection ss = new StringSelection(s); 
	    this.getToolkit().getSystemClipboard().setContents(ss,ss); 
	}
    } 
    
    public String getObjectAt(int location) throws BadLocationException{
	int objStart = 0, objEnd = 0;
	PlainDocument pDoc = (PlainDocument)getDocument();
	Element elem = pDoc.getParagraphElement(location);
	Element root = pDoc.getDefaultRootElement();
	int index = root.getElementIndex(location);

	Element current;
	int i = 0;
	while( (index - i) >= 0){
	    current = root.getElement(index-i);
	    if(getText(current.getStartOffset(), (current.getEndOffset() - 
						  current.getStartOffset())).trim().length() == 0){
		break;
	    }
	    else
		objStart = current.getStartOffset();
	    i++;
	}
	i = 0;
	while( (index + i) < pDoc.getLength()){
	    current = root.getElement(index+i);
	    if(getText(current.getStartOffset(), (current.getEndOffset() - 
						  current.getStartOffset())).trim().length() == 0){
		break;
	    }
	    else
		objEnd = current.getEndOffset();
	    i++;
	}
	if( getText(objStart, (objEnd - objStart)).trim().length() == 0)
	    return null;
	return getText(objStart, (objEnd - objStart));
    }

    public void mouseReleased(MouseEvent e){
    }

    public void mouseEntered(MouseEvent e){
    }

    public void mouseExited(MouseEvent e){
    }

    public void mousePressed(MouseEvent e){
    }
}

