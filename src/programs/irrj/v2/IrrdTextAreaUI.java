import javax.swing.plaf.basic.*;
import javax.swing.text.*;

public class IrrdTextAreaUI extends BasicTextAreaUI{

    public IrrdTextAreaUI(){
	super();
    }

    public View create(Element m){
	return new IrrdView(m);
    }
}
