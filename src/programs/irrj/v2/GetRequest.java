import java.awt.*;
import java.io.*;
import java.net.*;
import java.util.*;
import javax.swing.*;

public final class GetRequest {

    private Socket s;
    private DataInputStream in;
    private DataOutputStream out;
    
    private final String INIT_STRING = "!!\r\n!nIRRj2\r\n!uF=0\r\n";
    private final String OUT_ERROR_STRING = "Unable to open Data connection to ";
    private String server;
    private int    port;
    private int    type;

    private String DEFAULT_SERVER;
    private int    DEFAULT_PORT;

    static final int STAY_CONNECTED = 0;
    static final int DISCONNECT = 1;

    private String buffer = null;
    int error=0;
    String error_string = new String();
    
    final int BUF_SIZE = 400;
    
    public volatile boolean run = true;
    
    public GetRequest(String server_in, int port_in, int in_type){
	server = server_in;
	port = port_in;
	type = in_type;
	IRRj.log.append("Opening connection to " + server  +":"+ port + "\n");
	getConnection();
    }
    
   
    public void changeServer(String server_in, int port_in){
	quit(); 
	server = server_in;
	port = port_in;
	IRRj.log.append("Opening connection to " + server  +":"+ port + "\n");
	getConnection();
    }
    
    private void getConnection(){
	try {
	    s = new Socket(server,port);
	    s.setSoTimeout(10000);
	    in = new DataInputStream(s.getInputStream());
	    out = new DataOutputStream(s.getOutputStream());
	}
	catch (UnknownHostException e){
	    if(IRRj.optWin != null){
		IRRj.optWin.statLabel.setText(server + " is not a valid host");
		IRRj.optWin.tabbedPane.setSelectedIndex(0);
		IRRj.optWin.setVisible(true);
	    }
	    else
		System.out.print(e);
	    return;
	}
	catch (IOException e){
	    if(IRRj.optWin != null){
		IRRj.log.append(e + "\n");
		IRRj.optWin.statLabel.setText("IOException, check log");
		IRRj.optWin.tabbedPane.setSelectedIndex(0);
		IRRj.optWin.setVisible(true);
	    }
	    else
		System.out.print(e);
	    return;
	}
	
	if( (in == null) || (out == null) ){
	    IRRj.log.append("Stream NULL error\n");
	    if(IRRj.optWin != null){
		IRRj.optWin.statLabel.setText("Stream error, check log");
		IRRj.optWin.tabbedPane.setSelectedIndex(0);
		IRRj.optWin.setVisible(true);
	    }
	    else
		System.out.println("Stream error");
	    return;
	}
        if(type == 0){
	    try{
		byte b;
		out.writeBytes(INIT_STRING);
		out.flush();
		while((char)(b=in.readByte()) != '\n'); /* clear the C */
		while((char)(b=in.readByte()) != '\n'); /* clear the C */
	    }
	    catch(Exception e){
	      IRRj.log.append("Connection problem\n");
	    }
	}
	
    }
    
    public int getNewRequest(String request, JTextArea ita, JProgressBar jpb){
	int bytes=0;
	
	if((bytes=sendRequest(request, ita, jpb)) > -3)
	  return bytes;
	server = IRRj.prop.getProperty("queryserver", "whois.radb.net");
	port = Integer.valueOf(IRRj.prop.getProperty("queryport", "43")).intValue();
	if(IRRj.optWin != null){
	    IRRj.optWin.qstf.setText(server);
	    IRRj.optWin.qptf.setText(IRRj.prop.getProperty("queryport", "43"));
	}
	IRRj.log.append("Attempting restart to " +server+" "+port+"\n");
	getConnection();
	return sendRequest(request, ita, jpb);
    }
    
    public int getNewRequest(String request){
	int bytes=0;
	
	if((bytes=sendRequest(request)) > -3)
	    return bytes;
	server = IRRj.prop.getProperty("queryserver", "whois.radb.net");
	port = Integer.valueOf(IRRj.prop.getProperty("queryport", "43")).intValue();
	if(IRRj.optWin != null){
	    IRRj.optWin.qstf.setText(server);
	    IRRj.optWin.qptf.setText(IRRj.prop.getProperty("queryport", "43"));
	}
	IRRj.log.append("Attempting restart to " +server+" "+port+"\n");
	getConnection();
	return sendRequest(request);
    }

    public String getRawRequest(String req){
	String retValue = new String();

	try{	
	  if(type == 1){
	    
	    byte buffer[] = new byte[10000];
	    int temp_read = 0;
	    //Send the request to the server
	    if (out == null) return OUT_ERROR_STRING+server;
	    out.writeBytes(req+"\n");
	    out.flush();
	    try{
	      int num_times = 0;
	      while(in.available() == 0){
		num_times++;
		Thread.sleep(200);
		if(num_times > 10)
		  return("Timeout reading from host");
	      }
	    }
	    catch(InterruptedException e){}
	    catch(SocketException e){
	      retValue = "\nDouble check your server and port";
	      return retValue;
	    }
	    while( (temp_read = in.read(buffer)) > 0){
	      retValue = retValue + new String(buffer, 0, temp_read);
	    }
	  }
	else{ /* a query connection we can't expect to close */
	  out.writeBytes(req+"\n");
	  out.flush();
	  try{
	    while(in.available() == 0){
	      Thread.sleep(200);
	    }
	  }
	  catch(InterruptedException e){}
	  catch(SocketException e){
	    retValue = "\nDouble check your server and port";
	    return retValue;
	  }
	  byte byte_buffer[] = new byte[in.available()];
	  in.readFully(byte_buffer);
	  retValue = new String(byte_buffer);
	}
	}
	catch(SocketException e){
	 
	}
	catch(IOException e){
	    
	}

	return retValue;
    }

    public int sendRequest(String request, JTextArea ita, JProgressBar jpb){
	char initial = '-';
	byte b;
	int bytes = 0;
	String temp = new String();
	buffer = new String();
	run = true;
	request = request.trim();
	ita.setText("");

	try{
	    
	    //Send the request to the server
	    if (out == null) return -1;
	    out.writeBytes(request+IRRj.newLine);
	    out.flush();
	    
	    // Read in the first charachter returned
	    initial = (char)in.readByte();
	    
	    while((char)(b=in.readByte()) != '\n')
		temp=temp+(char)b;
	}
	catch(SocketException e){
	    IRRj.log.append(e + IRRj.newLine);
	}
	catch(IOException e){
	    IRRj.log.append(e + IRRj.newLine);
	}
	
	// If the first charachter returned is an A, as in A345, there are 345 bytes to read
	if(initial!='A'){
	    switch(initial){
		// If D is returned, the query was valid, but there was no data available.
	    case 'D':
		ita.setText("No objects found");
		return -1;
		// If F is returned, the query was invaild.
	    case 'F':
		ita.setText("Invalid query");
		return -2;
		// Otherwise there was some problem with the connection
	    case 'C':
		return 1;
	    case '%':
		ita.setText("No objects found");
	        return -1;
	    default:
		ita.setText("Connection problem");
		return -3;
	    }
	}
	
	// Determine how many bytes to read and create a new byte array
	bytes = Integer.parseInt(temp);
	jpb.setMaximum(bytes);

	byte byte_buffer[] = new byte[BUF_SIZE];
	
	int cumread = 1;
	//int cumread = 0;
	try {
	    // Read the stream in to the buffer, then create a new string
	    int numread;
	    String str;
	    boolean theseAreRoutes = false;

	    in.read(byte_buffer, 0, 1);
	    if(byte_buffer[0] == ' '){
		theseAreRoutes = true;
		((IrrdTextArea)ita).displayingRoutes(true);
	    }
	    else{
		str = new String(byte_buffer, 0, 1);
		ita.append(str);
		((IrrdTextArea)ita).displayingRoutes(false);
	      }
	    
	    while( (cumread < bytes-1) && run){
		if(bytes-cumread < BUF_SIZE)
		    numread = in.read(byte_buffer, 0, bytes-cumread);
		else
		    numread = in.read(byte_buffer, 0, BUF_SIZE);
		str = new String(byte_buffer, 0, numread);
		if(theseAreRoutes)
		    str = str.replace(' ', '\n');
		ita.append(str);
		cumread += numread;
		jpb.setValue(cumread);
		Thread.currentThread().yield();
		Thread.currentThread().sleep(10);  // it's more responsive with this here
	    }
	    if((b=in.readByte()) == 'C')
		in.readByte();             // Read in the C, then the \n
	    else
		while((char)(b=in.readByte()) != '\n'); // Read in until a newline
	}
	catch (Exception e){
	    IRRj.log.append(e + IRRj.newLine);
	}
	return cumread;
    }
    
    public int sendRequest(String request){
	char initial = '-';
	byte b;
	int bytes = 0;
	String temp = new String();
	buffer = new String();
	
	request = request.trim();
	
	try{
	    
	    //Send the request to the server
	    if (out == null) return -1;
	    out.writeBytes(request+"\n");
	    out.flush();
	    
	    // Read in the first charachter returned
	    initial=(char)in.readByte();
	    
	    while((char)(b=in.readByte()) != '\n')
		temp=temp+(char)b;
	}
	catch(SocketException e){
	    IRRj.log.append(e + IRRj.newLine);
	}
	catch(IOException e){
	    IRRj.log.append(e + IRRj.newLine);
	}
	
	// If the first charachter returned is an A, as in A345, there are 345 bytes to read
	if(initial!='A'){
	    switch(initial){
		// If D is returned, the query was valid, but there was no data available.
	    case 'D':
		return -1;
		// If F is returned, the query was invaild.
	    case 'F':
		return -2;
		// Otherwise there was some problem with the connection
	    case 'C':
		return 1;
	    case '%':
		return -1;
	    default:
		return -3;
	    }
	}
	
	// Determine how many bytes to read and create a new byte array
	bytes = Integer.parseInt(temp);
	byte byte_buffer[] = new byte[bytes];
	
	try {
	    // Read the stream in to the buffer, then create a new string
	    in.readFully(byte_buffer);
	    buffer = new String(byte_buffer);
	    
	    /* Read in the C\n (hopefully) leftover. (If it got this far, the request
	       should have been successfully handled) */
	    if((b=in.readByte()) == 'C')
		in.readByte();             // Read in the C, then the \n
	    else
		while((char)(b=in.readByte()) != '\n'); // Read in until a newline
	    
	}
	catch (Exception e){
	    IRRj.log.append(e + IRRj.newLine);
	}
	
	return bytes;
    }
    
    public String getSockInfo(){
	return (server);
    }
    
    public String getPort(){
	return Integer.toString(port);
    }
    
    public String getBuffer(){
	if (buffer.length()==0){
	    return null;
	}
	return buffer;
    }
    
    public void stopQuery(){
	run = false;
    }

    public void quit(){
	// Close the socket
	try{
	    out.writeBytes("!q\n");
	    s.close();
	}
	catch(Exception e){
	    //Debug.print(Debug.ERR, "GR Quit: "+e);
	}
	IRRj.log.append("Closing socket to: "+server+IRRj.newLine);
    }
    
    
} // End of Class Definition

