import javax.swing.*;
import javax.swing.text.*;

public class RpslObject{
    public static final int MANDATORY = 1;
    public static final int OPTIONAL = 2;
    public static final int MULTI = 3;
    public static final int SINGLE = 4;
    
    public int requiredPolicy;
    public int occurancePolicy;
    public String description;
    public JComponent panel;
    public String label;
    
    public RpslObject(int reqPol, int occPol, String key, String descr, JComponent p){
	requiredPolicy = reqPol;
	occurancePolicy = occPol;
	label = key;
	description = descr;
	panel = p;
    }
    
    public boolean dump(JTextArea where) {
	int timesThroughWhile = 0;
	if(((JTextComponent)panel).getText().trim().length() != 0){
	    if(occurancePolicy == SINGLE)
		where.append(makeLength(10,label));
	    int offset = 0, docLength, lineNo = 1;
	    PlainDocument doc = (PlainDocument)((JTextComponent)panel).getDocument();
	    docLength = doc.getLength();
	    while(offset < docLength){
		if(occurancePolicy == MULTI)
		    where.append(makeLength(10, label));
		else if(timesThroughWhile > 0)
		    where.append("\t\t");
		Element elem = doc.getParagraphElement(offset);
		try{
		    String str = doc.getText(elem.getStartOffset(), (elem.getEndOffset() - elem.getStartOffset())); 
		    offset += (elem.getEndOffset() - elem.getStartOffset());
		    where.append(str);
		}
		catch(BadLocationException ex){
		    break;
		}
		timesThroughWhile++;
	    }
	}
	else if(requiredPolicy == MANDATORY)
	    return false;
	    
	return true;
    }

    private String makeLength(int length, String input){
	if(length < input.length())
	    return input;
	
	int currentLength = input.length();
	int extendSpaces = length - currentLength;
	StringBuffer strbuf = new StringBuffer();
	strbuf.append(input);
	
	for(int i = 0; i < extendSpaces; i++)
	    strbuf.append(" ");
	
	return strbuf.toString();
    }
}
