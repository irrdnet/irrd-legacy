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


import java.io.*;
import java.util.*;
import netscape.security.*;

import debug.Debug;

// handles reading from and writing to the preferences file.

// for now, all exceptions cause meesages to be sent to the Debug object.
// at some point, some of these exceptions should probably show an alert
// dialog to the user. --mukesh

public class PreferenceManager {
  // when the attribute name contains illegal characters...
  static final String ERR_ATTRIB = "The attribute name is invalid: ";
  // when the module name contains illegal characters
  static final String ERR_MODULE = "The module name is invalid: ";

  // separates the module and attribute names
  static final String FIELD_SEPARATOR = ".";
  // separates the name from the value. this was determined empirically
  // under Sun's JDK 1.1.6
  static final char VALUE_PREFIX = '=';
  // the name of the preferences file
  static final String FILENAME = ".ipmarc";

  static PreferenceManager pm;
   
  String filePath;      
  Properties prefs;

  // using the "singleton" design pattern
  static public PreferenceManager getInstance() {
	 if (pm==null) {
		pm = new PreferenceManager();
	 }
      
	 return pm;
  }
      
  public String getGlobal(String attribute)
		 throws IllegalArgumentException {

			if (!checkName(attribute))
			  throw new IllegalArgumentException(ERR_ATTRIB + attribute);

			return (String) prefs.get(attribute);
  }

  public String get(String module, String attribute)
		 throws IllegalArgumentException {

			if (module==null)
			  throw new IllegalArgumentException(ERR_MODULE + module);
      
			if (!checkName(attribute))
			  throw new IllegalArgumentException(ERR_ATTRIB + attribute);
         
			if (!checkName(module))
			  throw new IllegalArgumentException(ERR_MODULE + module);

			return (String) prefs.get(module + FIELD_SEPARATOR + attribute);
  }

  public void setGlobal(String attribute, String value)
		 throws IllegalArgumentException {

			if (attribute == null)
			  throw new IllegalArgumentException(ERR_ATTRIB + attribute);

			if (!checkName(attribute))
			  throw new IllegalArgumentException(ERR_ATTRIB + attribute);

			prefs.put(attribute, value);
  }
   
  public void set(String module, String attribute, String value)
		 throws IllegalArgumentException {
 
			if (attribute == null)
			  throw new IllegalArgumentException(ERR_ATTRIB + attribute);
      
			if (module == null)
			  throw new IllegalArgumentException(ERR_MODULE + module);
      
			if (!checkName(attribute))
			  throw new IllegalArgumentException(ERR_ATTRIB + attribute);
      
			if (!checkName(module))
			  throw new IllegalArgumentException(ERR_MODULE + module);

			prefs.put(module + FIELD_SEPARATOR + attribute, value);
  }
    
  public void save() {

	 FileOutputStream prefsFile = null;

	 Debug.print(Debug.DETAILED, "saving preferences");

	 // browser mode, not appletviewer. enable netscape priveleges.
	 // for writing preferences file.
      
	 if ((!ipma.PlugIn.StartApps.isApplication) &&
		  (ipma.PlugIn.StartApps.isBrowser)) {
		try {
		  PrivilegeManager.enablePrivilege ("UniversalFileWrite");
		  Debug.print (Debug.DETAILED, "FileWrite Permission is enabled");
		}
		catch (netscape.security.ForbiddenTargetException err) {
		  Debug.print (Debug.ERR, "FileWrite Permission denied.");
		}
	 }

	 try {
		prefsFile = new FileOutputStream(filePath);      
		prefs.save(prefsFile, null);
	 } catch (java.io.IOException IOE) {
		Debug.print(Debug.TOPLEVEL, "preferencs not saved: "+IOE);
	 } catch (SecurityException SE) {
		Debug.print(Debug.TOPLEVEL, "preferences not saved: "+SE);
	 } finally {
		if (prefsFile != null)
		  try {
		  prefsFile.close();
		} catch (java.io.IOException IOE) {
		  Debug.print(Debug.TOPLEVEL, "preferences not saved: "+IOE);
		}
	 }
	 Debug.print(Debug.DETAILED, "saved preferences");

	 // browser mode, not appletviewer. disable netscape priveleges
	 // immediately so there's no room for hanky panky.
      
	 if ((!ipma.PlugIn.StartApps.isApplication) &&
		  (ipma.PlugIn.StartApps.isBrowser)) {
		try {
		  PrivilegeManager.disablePrivilege ("UniversalFileWrite");
		  Debug.print (Debug.DETAILED, "FileWrite Permission is disabled");
		}
		catch (netscape.security.ForbiddenTargetException err) {
		  Debug.print (Debug.ERR, "FileWrite Permission denied.");
		}
	 }

  }
   
  public void finalize() {
	 save();
  }

  protected PreferenceManager() {

	 FileInputStream prefsFile = null;
       
	 prefs = new Properties();
       
	 //System.out.println
	 if (ipma.PlugIn.StartApps.rcfile == null){
	   // browser mode, not appletviewer. enable 
	   // netscape priveleges to read the user.home
	   // property
	   
	   if ((!ipma.PlugIn.StartApps.isApplication) &&
	       (ipma.PlugIn.StartApps.isBrowser)) {
		  try {
			 PrivilegeManager.enablePrivilege 
				("UniversalPropertyRead");
			 Debug.print (Debug.DETAILED, 
							  "Prop Read Permission is enabled");
		  }
		  catch (netscape.security.ForbiddenTargetException err) {
			 Debug.print (Debug.ERR, 
							  "Prop Read Permission denied.");
		  }
	   }
	   filePath =
		  System.getProperty("user.home") +
		  System.getProperty("file.separator") +
		  FILENAME;

	   Debug.print (Debug.DETAILED, "Default: rc file path = " +
						 filePath);

	   // browser mode, not appletviewer. disable 
	   // netscape priveleges immediately so there's 
	   // no room for hanky panky.
	   
	   if ((!ipma.PlugIn.StartApps.isApplication) &&
	       (ipma.PlugIn.StartApps.isBrowser)) {
		  try {
			 PrivilegeManager.disablePrivilege 
				("UniversalPropertyRead");
			 Debug.print (Debug.DETAILED, 
							  "Prop Read Permission is disabled");
		  }
		  catch (netscape.security.ForbiddenTargetException err) {
			 Debug.print (Debug.ERR, 
							  "Prop Read Permission denied.");
		  }
	   }
	   
	   
	 }
       
	 else{
	   // there are several cases
	   // 1. rcfile doesn't contain any path seperator
	   //    This means it is a file in the current diretory
	   //    We will get the current working directory and concatenate with the file name
	   //
	   // 2. rcfile contains path seperator and the last char is a path seperator
	   //    This means it is a path rather than a file name
	   //
	   // 3. rcfile contains path seperator and it doesn't end with a path seperator
	   //    Add a path seperator to the end and add ".ipmarc"
	   //
	   // Since there is no way of knowing whether a path is a file or a directory
	   // I don't check for that.  If it contains file seperators, the file name 
	   // will be .ipmarc
	   
	   String rc = ipma.PlugIn.StartApps.rcfile;
	   // browser mode, not appletviewer. enable 
	   // netscape priveleges to read the user.home
	   // property
	   
	   if ((!ipma.PlugIn.StartApps.isApplication) &&
	       (ipma.PlugIn.StartApps.isBrowser)) {
		  try {
			 PrivilegeManager.enablePrivilege 
				("UniversalPropertyRead");
			 Debug.print (Debug.DETAILED, 
							  "Prop Read Permission is enabled");
		  }
		  catch (netscape.security.ForbiddenTargetException err) {
			 Debug.print (Debug.ERR, 
							  "Prop Read Permission denied.");
		  }
	   }
	   String pathSep = System.getProperty("file.separator");
	   String currentDir = System.getProperty("user.dir");
	   
	   if ((!ipma.PlugIn.StartApps.isApplication) &&
	       (ipma.PlugIn.StartApps.isBrowser)) {
		  try {
			 PrivilegeManager.disablePrivilege 
				("UniversalPropertyRead");
			 Debug.print (Debug.DETAILED, 
							  "Prop Read Permission is disabled");
		  }
		  catch (netscape.security.ForbiddenTargetException err) {
			 Debug.print (Debug.ERR, 
							  "Prop Read Permission denied.");
		  }
	   }
	   
	   //System.out.println("currentDir is " + currentDir);
	   
	   // Case 1
	   if (rc.indexOf(pathSep) == -1){
		  //System.out.println("currentDir is " + currentDir);
		  filePath = completePath(currentDir) + FILENAME;
	   }
	   
	   else{
		  System.out.println ("complete path is " + 
									 completePath (rc));
	       
		  filePath = completePath(rc) + FILENAME;
	   }
	 }
       
	 //System.out.println("Loading preferencees from " + filePath);
       
	 Debug.print(Debug.DETAILED, "loading preferences from " + filePath);
       
	 // browser mode, not appletviewer. enable netscape priveleges.
       
	 if ((!ipma.PlugIn.StartApps.isApplication) &&
		  (ipma.PlugIn.StartApps.isBrowser)) {
	   try {
		  PrivilegeManager.enablePrivilege ("UniversalFileRead");
		  Debug.print (Debug.DETAILED, "File Read Permission is enabled");
	   }
	   catch (netscape.security.ForbiddenTargetException err) {
		  Debug.print (Debug.ERR, "File Read Permission denied.");
	   }
	 }
       
	 try {
	   prefsFile = new FileInputStream(filePath);
	   prefs.load(prefsFile);
	 } catch (java.io.FileNotFoundException FNFE) {
	   Debug.print(Debug.TOPLEVEL, "preferences not loaded: "+FNFE);
	 } catch (java.io.IOException IOE) {
	   Debug.print(Debug.TOPLEVEL, "preferences not loaded: "+IOE);         
	 } catch (SecurityException SE) {
	   // keep track of this, so we don't try again?      
	   Debug.print(Debug.TOPLEVEL, "preferences not loaded: "+SE);
	 } finally {
	   try {
		  if (prefsFile != null)
			 prefsFile.close();
	   } catch (java.io.IOException IOE) {
		  // this doesn't really matter AFAIK -- mukesh
		  Debug.print (Debug.ERR, "IO Exception in Loading Preferences");
	   }
	 }
	 Debug.print(Debug.DETAILED, "loaded preferences");
       
       
	 // browser mode, not appletviewer. disable netscape priveleges
	 // immediately so there's no room for hanky panky.
       
	 if ((!ipma.PlugIn.StartApps.isApplication) &&
		  (ipma.PlugIn.StartApps.isBrowser)) {
	   try {
		  PrivilegeManager.disablePrivilege ("UniversalFileRead");
		  Debug.print (Debug.DETAILED, 
							"File Read Permission is disabled");
	   }
	   catch (netscape.security.ForbiddenTargetException err) {
		  Debug.print (Debug.ERR, "File Read Permission denied.");
	   }
	 }
       
  }		

  protected boolean checkName(String prefName) {
	 if ((prefName.indexOf(FIELD_SEPARATOR) != -1) ||
		  (prefName.indexOf(VALUE_PREFIX) != -1))
		return false;
	 
	 return true;
  }
  
  private String completePath(String pathname){
      
	 // browser mode, not appletviewer. enable 
	 // netscape priveleges to read the user.home
	 // property
      
	 if ((!ipma.PlugIn.StartApps.isApplication) &&
		  (ipma.PlugIn.StartApps.isBrowser)) {
		try {
		  PrivilegeManager.enablePrivilege ("UniversalPropertyRead");
		  Debug.print (Debug.DETAILED,"Prop Read Permission is enabled");
		}
		catch (netscape.security.ForbiddenTargetException err) {
		  Debug.print (Debug.ERR,"Prop Read Permission denied.");
		}
	 }
	
	 String pathSep = System.getProperty("file.separator");

	 // browser mode, not appletviewer. disable netscape priveleges
	 // immediately so there's no room for hanky panky.
      
	 if ((!ipma.PlugIn.StartApps.isApplication) &&
		  (ipma.PlugIn.StartApps.isBrowser)) {
		try {
		  PrivilegeManager.disablePrivilege ("UniversalPropertyRead");
		  Debug.print (Debug.DETAILED, "Prop Read Permission is disabled");
		}
		catch (netscape.security.ForbiddenTargetException err) {
		  Debug.print (Debug.ERR, "Prop Read Permission denied.");
		}
	 }	 

	 if (pathname.endsWith(pathSep)){
		return pathname;
	 }
	 else{
		return(pathname + pathSep);
	 }
  }

  /* This function is only called for debugging purposes to make
	* sure the preference manager is loading the values correctly.
	*/

  public void printAll () {
	
	 System.out.println ("printAll: Prefs table contains ");
	 Debug.print (Debug.TOPLEVEL, prefs.size() + " keys");
	 Enumeration e = prefs.propertyNames();
	 while (e.hasMoreElements()) {
		String key = (String) e.nextElement();
		System.out.println ("KEY: " + key);
		System.out.println ("VALUE: " + prefs.get (key));
	 }
  }

	 
}











