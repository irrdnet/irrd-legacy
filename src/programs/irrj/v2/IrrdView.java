import javax.swing.text.*;
import java.awt.*;


public class IrrdView extends PlainView {

    public IrrdView(Element elem){
	super(elem);
    }

    protected int drawUnselectedText(Graphics g, int x, int y, int p0, int p1)
	throws BadLocationException{
	g.setColor(Color.black);
	Document doc = getDocument();
	Segment lineBuffer = new Segment();// = getLineBuffer();
	doc.getText(p0, p1 - p0, lineBuffer);
	String str = lineBuffer.toString();
	if(str.startsWith("route:") || str.startsWith("origin:") 
	   || str.startsWith("mnt-by:") || Character.isDigit(lineBuffer.array[0]) ){
	    g.setColor(Color.blue);
	    int r = Utilities.drawTabbedText(lineBuffer, x, y, g, this, p0);
	    g.setColor(Color.black);
	    return r;
	}
	return Utilities.drawTabbedText(lineBuffer, x, y, g, this, p0); 
    }
}
