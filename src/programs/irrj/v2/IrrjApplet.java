import javax.swing.*;
 
public class IrrjApplet extends JApplet implements Runnable{
 
    public IrrjApplet(){
        new Thread(this).start();
    }
 
    public void run(){
        new IRRj();
    }
}                       
