package ipma.Util;

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




import java.io.* ;
import java.net.* ;


/**
 * SendMail
 * 
 * The SendMail class provides a simple mechanism for sending
 * e-mails using SMTP.
 * 
 * Modification History:
 *
 * @version 1.0 1996/Sep/17
 * @version 1.1 
 * @date 23 April 1998
 * @author	Rajiv Pant (Betul)  betul@rajiv.com  http://rajiv.com
 * @author port to java 1.1 by Sushila Subramanian 
 */


public class SendMail
{
  
  
  
  // The variable $DEBUG is set to true to view responses from the
  // SMTP server while debugging/testing this class.
  
  final static boolean $DEBUG = false ;
  //	final static boolean $DEBUG = true ;
  
  // By the way, did you know that variables names in Java
  // can also begin with  a dollar sign ($) like in Perl?
  
  
  final static int SMTPPort = 25 ;
  
  String SMTPServer = null ;
  
  String Headers = null ;
  String Body = null ;
  
  String From = null ;
  
  String To = null ;
  // Additional To: headers may be specifed using the
  // setHeaders (String) method.
  
  /*
    
    The next version of this program will provide methods
    to set, and get each of these common headers indivdually
    to make it even easier to use this class.
    These varibles are currently unused.
    
    String ReplyTo = null ;
    String Cc = null ;	// Can be multiple.
    String Bcc = null ;	// Can be multiple.
    String Subject = null ;
    
    String X-Mailer = null ;
    
    */
  
  
  
  
  
  /**
    The setSMTPServer(String) method is used to set the name of the
    SMTP server the deliver method will use to deliver the message.
    
    @param	host	The host to use as the SMTP server.
    */
  
  public void setSMTPServer (String host)
  {
    SMTPServer = host ;
  } // end setSMTPServer (String)
  
  

  
  
  /**
    The getSMTPServer() method returns the name of the currently
    set SMTP Server.
    */
  
  public String getSMTPServer ()
  {
    return SMTPServer ;
  } // end getSMTPServer()
  
  
  
  
  /**
    The setFrom (String) method sets the "From" address used
    in the "MAIL FROM: " part of the SMTP handshake.
    
    @param	address	The e-mail address of sender of the message.
    */

  
  public void setFrom (String address)
  {
    From = address ;
  } // end setFrom (String)
  
  
  
  /**
    The setTo (String) method sets the "To" address used
    in the "RCPT TO: " part of the SMTP handshake.
    
    @param	address	The e-mail address of the message recipient.
    */
  
  
  public void setTo (String address)
  {
    To = address ;
  } // end setTo (String)
  
  
  
  
  /**
    The setHeaders (String) method sets the message's headers.
    Each header is separated by a new line.
    <br>
    The "From: " and a "To: " need to be specified even though
    the From and To used in the SMTP handshake are set by the
    setFrom (String) and setTo (String) methods.

    <pre>
    --- begin example ---
    From: Rajiv Betul <betul@rajiv.org>
    To: Bill Gates <William.Henry.Gates.III@microsoft.com>
    Cc: Scott McNealey <Scott.McNeally@sun.com>
    Bcc: Cool Person <cool@cool-people.com>
    Subject: Tomorrow's Party
    --- end example ---
    <pre>
    
    @param	text	The message headers.
    */
  
  public void setHeaders (String text) {
    Headers = text ;
  } // end setHeaders (String)

  /**
    The setBody (String) method sets the message's body text.
    
    <pre>
    TO DO: Check the body text to make sure it does not contain
    a lone period in a line by itself.
    Also fix lines that begin with a "From ".
    </pre>
    
    @param	text	The message body.
    */


  public void setBody (String text) {
    Body = text ;
  } // end setBody (String)
  
  
  
  
  /**
    The checkServerResponse (String, String) method checks if the
    response line from the server starts with the expected
    response code.
    */
  
  static void checkServerResponse (String SMTPServer, 
				   String ServerResponseLine, String ExpectedResponseCode)
    throws Exception {
      
      if ($DEBUG)
	System.out.println (ServerResponseLine) ;
      
      if (! ServerResponseLine.startsWith (ExpectedResponseCode) )
	throw new Exception(ServerResponseLine) ;
  } // end checkServerResponse (String, String)
  
  
  

  
  /**
    The deliver() method connects to the SMTP server and delivers the message.
    The String it returns contains any responses it gets from the
    SMTP server while trying to deliver the e-mail.
    <br>
    This method does not retry or queue the message for later delivery
    if the connection fails.
    I may upgrade it later to become more fail-safe.
    <br>
    For more information on SMTP, refer to the following documents:
    <br>
    RFC822 (Message Format)
    ftp://ds.internic.net/rfc/rfc822.txt
    <br>
    RFC821 (SMTP Protocol)
    ftp://ds.internic.net/rfc/rfc821.txt
    <br>
    RFC1891 - 1894 (ESMTP Extensions)
    ftp://ds.internic.net/rfc/rfc1891.txt
    
    @exception SendMailCouldNotDeliverException when the server could not deliver the message.
	*/
  
  public void deliver() throws Exception {
    
    Socket SMTPSocket = null ;
    String ServerResponseLine ;
    
    // This block is used to test the throwing of exceptions.
    
    if ($DEBUG) {
      if ( SMTPServer.equals("fake") )
	throw new Exception (
			     "The server name " + SMTPServer + " is not allowed for testing.");
    }
    
    
    
    
    try {
      // Making the network connection.
      SMTPSocket = new Socket (SMTPServer, SMTPPort) ;
      
      // 1.1 port - PrintStream replaced by PrintWriter
      //      PrintStream ClientSays =
      //	new PrintStream(SMTPSocket.getOutputStream());

      PrintWriter ClientSays =
	new PrintWriter (SMTPSocket.getOutputStream());
      
      //DataInputStream ServerResponds =
      //new DataInputStream(SMTPSocket.getInputStream());

      BufferedReader ServerResponds = 
	new BufferedReader(new InputStreamReader(SMTPSocket.getInputStream()));

      // The SMTP dialog begins.
      
      // First we wait for the server to say "220"
      // need the sleep here - coz readLine is a blocking call.
      int j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "220") ;
      
      // Then we say HELO
      ClientSays.println ("HELO " + SMTPServer) ;
      ClientSays.flush();

      j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "250") ;
      
      ClientSays.println ("MAIL FROM: <" + From + ">") ;
      ClientSays.flush();

      j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "250") ;
      
      ClientSays.println ("RCPT TO: <" + To + ">") ;
      ClientSays.flush();

      j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "250") ;
      
      ClientSays.println ("DATA") ;
      ClientSays.flush();

      j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "354") ;
      
      
      // Sending Message Headers
      ClientSays.println (Headers) ;
      ClientSays.flush();

      // Making sure there is a newline after the last header.
      if (! Headers.endsWith("\n") ) {
	ClientSays.println ("\n") ;
	ClientSays.flush();
      }
      
      // Followed by a blank line that separates headers from body.
      ClientSays.println () ;
      ClientSays.flush();

      // Sending Message Body Text
      ClientSays.println (Body) ;
      ClientSays.flush();

      // Making sure there is a newline after the body text.
      if (! Body.endsWith("\n") ) {
	ClientSays.println ("\n") ;
	ClientSays.flush();
      }
      
      // Sending a lone period (.) in a line.
      // This signifies the end of the body text.
      
      // TO DO: Earlier in the process check the body text
      // to make sure it does not contain a lone period in a line
      // by itself. Also fix lines that begin with a "From ".
      // Preferably, do these in the setBody(String) method.
      
      ClientSays.println (".") ;
      ClientSays.flush();

      j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "250") ;
      
      
      // Ending the SMTP session.
      
      ClientSays.println ("QUIT") ;
      ClientSays.flush();

      j = 0;
      while (!ServerResponds.ready()) {
	try {
	  Thread.sleep (100);
	} catch (InterruptedException err) {}
	if (j++ > 10) break;
      }
      ServerResponseLine = ServerResponds.readLine() ;
      checkServerResponse (SMTPServer, ServerResponseLine, "221") ;
      
      
    } catch (Exception e) {
      throw new Exception (
			   "Could not close socket, " + 
			   "but mail may have been sent ok.") ;
      
    } finally {
      try {
	SMTPSocket.close() ;
      } catch (Exception e2) {
	throw new Exception (
			     "Could not close socket, " + 
			     "but mail may have been sent ok.") ;
      }
      
    } // end finally
    
  } // end deliver() ;
  
  
} // end class SendMail
