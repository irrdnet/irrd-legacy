package irr;

/*
 * Copyright (c) 1997, 1998
 *      The Regents of the University of Michigan ("The Regents").
 *      All rights reserved.
 *
 * Contact: ipma-support@merit.edu
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      Michigan and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *    
 */   




/**
 * GetRequest
 * 
 *  Sends specific requests for information based on channel
 *  and date to the server, and returns responses.
 * 
 * 
 * Modification History:
 * Added ability to send requests
 * directly to server
 * @May, 1999
 * @author Jon Poland
 *
 * Port to jdk1.1.
 * @version 1.1.
 * @date 11 June 1998.
 * @author Sushila Subramanian.
 *
 * @version 1.0
 * @date 
 * @author Aaron Klink.
 */



import java.awt.*;
import java.io.*;
import java.net.*;
import java.util.*;
//import ipma.Window.*;
//import ipma.Util.*;
//import debug.*;

public final class GetRequest extends Thread{

    private Socket s;
    private DataInputStream in;
    private PrintWriter out;
    
    private final String INIT_STRING = "!!\r\n!nIRRj\r\n!uF=0\r\n";
    private final String OUT_ERROR_STRING = "Unable to open Data connection to ";
    private String server;
    private int    port;
    private int    type;

    private String DEFAULT_SERVER;
    private int    DEFAULT_PORT;

    private String buffer = null;
    int error=0;
    String error_string = new String();
    
    private boolean $DEBUG = false;
    
    // End of variables
    
    public GetRequest(String server_in, int port_in, int in_type){
	server = server_in;
	port = port_in;
	type = in_type;
	System.err.println("Opening connection to "+server+":"+port);
	
	getConnection();
    }
    
   
    public void changeServer(String server_in, int port_in){
	quit(); 
	server = server_in;
	port = port_in;
	System.err.println("Opening connection to "+server+":"+port);
	getConnection();
    }
    
    private void getConnection(){
	// Open a connection and related data streams to the server.

	try {
	    s = new Socket(server,port);
	    s.setSoTimeout(10000);
	    in = new DataInputStream(s.getInputStream());
	    out = new PrintWriter(s.getOutputStream());
	}
	catch (UnknownHostException e){
	    error=1;
	    error_string = server+" is not a known host.";
	    return;
	}
	catch (IOException e){
	    error = 1;
	    error_string="IOException: " + e;
	    return;
	}
	
	if( (in == null) || (out == null) ){
	    error=1;
	    error_string=" stream null error";
	    return;
	}
        if(type == 0){
	    try{
		byte b;
		out.print(INIT_STRING);
		out.flush();
		while((char)(b=in.readByte()) != '\n'); /* clear the C */
		while((char)(b=in.readByte()) != '\n'); /* clear the C */
	    }
	    catch(Exception e){
	      System.out.print("Connection problem\n");
	    }
	}
	
    }
    
    public int getNewRequest(String request){
	int bytes=0;
	
	if((bytes=sendRequest(request)) > -3)
	  return bytes;
	server = IRRWindow.DEFAULT_SERVER;
	port = IRRWindow.DEFAULT_PORT;
	System.out.println("Attempting restart to "+server+" "+port+"\n");
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
	    out.print(req+"\n");
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
	  out.print(req+"\n");
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

    public int sendRequest(String request){
	char initial = '-';
	byte b;
	int bytes = 0;
	String temp = new String();
	buffer = new String();
	
	request = request.trim();
	
	if($DEBUG)System.out.println("SENDING: "+request);
	
	try{
	    
	    //Send the request to the server
	    if (out == null) return -1;
	    out.print(request+"\n");
	    out.flush();
	    
	    // Read in the first charachter returned
	    initial=(char)in.readByte();
	    
	    while((char)(b=in.readByte()) != '\n')
		temp=temp+(char)b;
	}
	catch(SocketException e){
	    System.out.println(e);
	}
	catch(IOException e){
	    System.out.println(e);
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
		if($DEBUG)System.out.println("\nIN: "+initial);
		return -3;
	    }
	}
	
	// Determine how many bytes to read and create a new byte array
	bytes = Integer.parseInt(temp);
	if($DEBUG)System.out.print("Reading "+ bytes + " bytes\n");

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
	    e.printStackTrace();
	}
	
	if($DEBUG)System.out.println(bytes+" bytes read in.");
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
    
    public void quit(){
	
	// Close the socket
	try{
	    out.println("!q");
	    s.close();
	}
	catch(Exception e){
	    //Debug.print(Debug.ERR, "GR Quit: "+e);
	}
	System.out.print("Closing socket to: "+server+"\n");
    }
    
    
} // End of Class Definition

