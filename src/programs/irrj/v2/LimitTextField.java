import javax.swing.*;
import javax.swing.text.*;
import java.awt.Toolkit;

class LimitTextField extends JTextField{
    int max;
    int max_int;
	
    public LimitTextField(int cols){
	super(cols);
	max = cols;
    }

    public LimitTextField(int cols, int int_limit){
	super(cols);
	max = cols;
	max_int = int_limit;
    }

    protected Document createDefaultModel() {
	return new CodeDocument();
    }
    
    //inner class
    protected class CodeDocument extends PlainDocument {
	int j = 0;
	char[] result = null;
	public void insertString(int offs, String str, AttributeSet a) throws BadLocationException {
	    char[] source = str.toCharArray();
	    if(offs < max) {
		result = new char[source.length];
		j = 0;
		for (int i = 0; i < result.length; i++) {
		    if(Character.isDigit(source[i]))
			result[j++] = source[i];
		    else{
			Toolkit.getDefaultToolkit().beep();
			return;
		    }	
		}
		
	    } else {
		Toolkit.getDefaultToolkit().beep();
		return;
	    }
	    String newString = new String(result, 0, j);
	    
	    if(max_int > 0){
		int docLen = super.getLength();
		String strDoc = super.getText(0, docLen) + newString;
		try{
		    if( (docLen > 0) && (Integer.valueOf(strDoc).intValue() > max_int) ){
			Toolkit.getDefaultToolkit().beep();
			return;
		    }	
		}
		catch(NumberFormatException e){e.printStackTrace();}
	    }

	    super.insertString(offs, newString, a);
         }
    }
}
    

