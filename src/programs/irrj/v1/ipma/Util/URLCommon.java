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


/** Set of common HTTP routines likely to be used by multiple routines.
 * 
 *  $Log:
 */

import java.util.*;
import java.awt.*;
import java.text.*;
import java.net.*;
import java.io.*;
import java.awt.event.*;

import netscape.security.*;
import ipma.PlugIn.*;
import debug.*;

public class URLCommon {
   

  /* intention is for this class to do both a read and 
   *  write. so far HTTP_Get will just read if the query
   *  parameter is null, and will write a query, and read
   *  a response if query is non-null. need to add another
   *  routine, that will just write a remote file to make
   *  this object complete.
   */

  // menu containing list of URLs to remote sites.
  protected static Menu siteMenu = null;
  public static ActionListener listener = null;

  /**
	* Creates a URLConnection object for a given URL value.
	* @param urlStr String name of URL.
	* @return HttpURLConnection, if successful.
	* @throw Exception.
	*/

  public static URLConnection initializeURL (String urlStr)
		 throws Exception{
			URL url = null;
			try {
			  url = new java.net.URL (urlStr);
			}
			catch (MalformedURLException me) {
			  if (urlStr != null) {
				 System.err.println ("Malformed URL: " + urlStr);
			  }
			  throw new MalformedURLException (urlStr);
			}
      
			try {
			  URLConnection uConn = url.openConnection();
			  return uConn;
			}
			catch (IOException ie ) {
			  String err = (" " + ie);
			  System.err.println ("IOException in open conn");
			  throw new IOException (err);
			}
      
  }



  /** HTTP_Get makes an HTML GET query to the given URL, and waits
	*  for the response from the CGI script running at the URL.
	* 
	*  @param urlConn URL Connection to get input from.
	*  @param query String for query.
	*  @param httpconn Boolean indicating if this is clueless netscape mode
	*         or a more intelligent Application mode that understands 
	*         and implements HttpURLConnection properly.
	*/

  public static String HTTP_Get (URLConnection urlConn, String query,
											boolean httpconn)
		 throws Exception {

			HttpURLConnection hConn = null;
			String response = null;
      
			/* Set the HTTP query to be a "GET" */
      
			if (httpconn) {
			  hConn = (HttpURLConnection) urlConn;
			}

			try {
			  if (httpconn) {
				 hConn.setRequestMethod ("GET");
			  }

			  else {
				 urlConn.setRequestProperty ("content-type", "application/x-www-form-urlencoded");
				 urlConn.setUseCaches (false);
			  }
			}
			catch (ProtocolException pe) {
			  System.err.println ("Protocol Error in GET");
			  throw new ProtocolException ("GET: Protocol Error" + pe);
			}
      
			/* Write the arguments via the output stream to the URL */

			try {

			  if (query != null) 
				 urlConn.setDoOutput (true);
			  urlConn.setDoInput (true);

			  // if this is a netscape browser - then set privileges

			  if ((!StartApps.isApplication) && (StartApps.isBrowser)) {
				 try {
               PrivilegeManager.enablePrivilege ("UniversalConnect");
               Debug.print (Debug.DETAILED, "Permission is enabled");
				 }
				 catch (netscape.security.ForbiddenTargetException err) {
               System.err.println ("Permission denied.");
				 }

			  }

			  OutputStream outStr = null;
			  OutputStreamWriter outWriter = null;

			  if (query != null) { 
				 outStr = urlConn.getOutputStream();
				 outWriter = new OutputStreamWriter (outStr);
				 outWriter.write (query, 0, query.length ());
				 outWriter.flush();
			  }
         

			  /* Read in the response */
			  if ((query != null) && httpconn) {
				 response = hConn.getResponseMessage ();
				 Debug.print (Debug.DETAILED, "response = " + response);
				 try {
               Thread.sleep (1000);
				 }
				 catch (InterruptedException e) {}
			  }

			  /* Create a Reader to read in the results via the input 
				* stream in the URL connection.
				*/

			  InputStream inStr = urlConn.getInputStream();
			  InputStreamReader inReader = new InputStreamReader (inStr);
			  BufferedReader br = new BufferedReader (inReader);
         
			  String oneLine;
			  while (isReady(br)) {
				 oneLine = br.readLine();
				 if (oneLine != null) {
					if (response == null){
					  response = oneLine;
					}
					else{
					  response = response + "\n" + oneLine;
					}
				 }

				 try {
               Thread.sleep (500);
				 }
				 catch (InterruptedException e) {}
			  }

			  if (query != null) {
				 outStr.close();
			  }
			  inStr.close ();
         
			  return response;

			}
			catch (IOException ie) {
			  System.err.println ("IO Exception in GET response, e = " + ie);
			  throw new IOException ("HTTP Get: IO Exception");
			}
  }

  /** isReady checks if BufferedReader has any data to read in a 2
	*  second timeframe. If it does - it returns true, else false.
	*/

  public static boolean isReady(BufferedReader br) {
	 int i = 0;
      
	 try {
		while (! br.ready ()) {
		  try {
			 Thread.sleep (200);
		  } catch (InterruptedException e) {}
		  if (i++ > 10) return false;
		}
	 }
	 catch (IOException ie) {
		System.err.println ("IO Exception for buffered ready");
		return false;
	 }

	 return true;
  }



  /** getAvailableSites looks for a file at the given URL
	*  that contains the possible URLs to access for IPMA data.
	*  If they are valid URLs add them to a Menu and return
	*  Menu to calling function.
	*  Valid URLs in Java must start with http:// blah blah..
	*  or file:// blah. Browser style expansion doesnt work.
	* 
	*  @param url URL to connect to.
	*  @return menu Menu object containing valid entries, or null
	*  if nothing existed.
	*/

  public static Menu getAvailableSites (String urlStr) {
	 URLConnection urlconn = null;
	 URLConnection tmpconn = null;
	 boolean isStupid = false;
	 String response = null;

	 /* do this once in the lifetime of the tools. if it already
	  * exists - dont bother looking at the file again. If the 
	  * list changes at Merit/Umich - its ok to restart the tool
	  * to bring in the new changes.
	  */

	 /*
		if ((siteMenu != null) && (siteMenu.getItemCount() > 0)) {
		return siteMenu;
		}
		*/

	 siteMenu = new Menu("Data Site");

	 if (siteMenu != null && siteMenu.getItemCount() > 0){
		/* We will not make a new connection */
		/* Just get all items in the menu and inserted them into the new menu */
		int size = siteMenu.getItemCount();
		MenuItem menuItem;

		for (int i = 0; i < size; i++){
		  menuItem = new MenuItem(siteMenu.getItem(i).getLabel());
		  siteMenu.add(menuItem);
		}

		return siteMenu;
	 }

	 try {
		urlconn = initializeURL (urlStr);
	 }
	 catch (Exception e) {
		// couldnt connect to base Merit/Umich file with list
		System.err.println ("Couldnt connect to Merit");
		return null;
	 }
      
	 // check if this is in browser mode or not.
	 if ((!StartApps.isApplication) && (StartApps.isBrowser)) {
		isStupid = true;
	 }

	 try {
		response = HTTP_Get (urlconn, null, isStupid);
	 }
	 catch (Exception e) {
		//either Protocol or IO error, regardless return null
		return null;
	 }

	 // response must be ok - parse it and make sure URLs are
	 // ok before adding to the menu.

	 if (response != null){
		String aURL;
		MenuItem mi;
		StringTokenizer strTok = new StringTokenizer (response, "\n");
		while (strTok.hasMoreTokens()) {
		  aURL = strTok.nextToken();
			
		  try {
			 if (aURL != null){
				tmpconn = initializeURL (aURL);
				// got this far, URL  must be ok.
			  
				if (siteMenu == null) {
				  siteMenu = new Menu("Data Site");
				}
				mi = new MenuItem (aURL);
				siteMenu.add (mi);
			 }
		  }
		  catch (Exception e) {
			 //continue to next item;
			 if (aURL != null) {
				System.err.println ("Invalid URL at " + aURL);
			 }
		  }
		}
	 }

	 if (siteMenu.getItemCount() == 0) {
		siteMenu = null; // no sense in sending an empty menu
	 }

	 if (listener != null){
		int size = siteMenu.getItemCount();
		
		for (int i = 0; i < size; i++){
		  MenuItem currentItem = siteMenu.getItem(i);
		  
		  currentItem.addActionListener(listener);
		}
	 }

	 return siteMenu;
  }

}
