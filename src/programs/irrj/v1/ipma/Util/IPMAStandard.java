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
 * IPMAStandard
 * Modified from UARCStandard.java
 *
 * Upper Atmospheric Research Collaboratory
 * School of Information
 * University of Michigan at Ann Arbor
 * 610 East University, Ann Arbor, MI 48109
 *
 *
 * Copyright (c) 1998 The Regents of The University of Michigan.
 * All rights reserved.
 *
 * Contact: jbres@umich.edu
 *
 *
 * A collection of static methods that implement UARC Standards.
 *
 * Modification History:
 * $Log: IPMAStandard.java,v $
 * Revision 1.1  2000/08/19 14:35:52  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:07  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:10  polandj
 * Adding irrj under repository
 *
 * Revision 1.6  1999/03/10 18:57:15  sushila
 * *** empty log message ***
 *
 * Revision 1.5  1999/02/02 07:34:11  vecna
 * No changes...
 *
 * 
 *    18/5/98: Sushila Subramanian
 *    Modified from UARC version to suit IPMA needs.
 *    Added extra functions to deal with date formatting in IPMA
 *    
 *    9/3/97: Jason Breslau
 *        Added dateToString
 *
 * @version @#@
 * @date 9/3/97
 * @author J. Breslau
 */

package ipma.Util;

import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.text.*;
import java.util.*;
import java.lang.reflect.*;

import debug.*;

public class IPMAStandard {
	
	/**
	 * Private constructor to prevent instantiation.
	 * All methods are static.
	 *
	 */
	private IPMAStandard() {}
	
	/** asYYMMDD and asMMDDYY have been incorporated from 
	 *  the old DateUtil routines and convert a date to the 
	 * IPMA formats.
	 *  @param d Date to be formatted.
	 *  @param seperator String designated as seperator. Ignored
	 *         right now.
	 *  @return String with the newly formatted date.
	 */
	
	static public String asYYMMDD(Date d) {
		return asYYMMDD(d, null);
	}
	
	static public String asYYMMDD(Date d, String separator) {
		String yrStr, monthStr, dateStr;
		
		SimpleDateFormat sdf = new SimpleDateFormat ("yyMMdd");
		// set UT
		sdf.setTimeZone(getUT());
		
		return (sdf.format (d));

		/* 
		// Get this DateFormat's calendar.
		Calendar c = sdf.getCalendar();
		
		// set the time.
		c.setTime(d);
		yrStr = new String ("" + (c.get (Calendar.YEAR) - 1900));
		int month  = c.get (Calendar.MONTH);
		if (month < 10)
		monthStr = new String ( "0" + month);
		else
		monthStr = new String ("" + month); 
		
		int datenum = c.get (Calendar.DAY_OF_MONTH);
		if (datenum < 10) 
		dateStr = new String ("0" + datenum);
		else 
		dateStr = new String ("" + datenum);
		return (df.format(d));
		*/

	}
	
	static public String asMMDDYY(Date d) {
		return asMMDDYY(d, null);
	}
	
	static public String asMMDDYY(Date d, String separator) {
		String yrStr, monthStr, dateStr;
		
		
		SimpleDateFormat sdf = new SimpleDateFormat ("MMddyy");
		// set UT
		sdf.setTimeZone(getUT());
		
		return (sdf.format (d));

		/*
		// Get a nifty date formatter.  This handles the "20-May-74" type thing.
		DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
		
		// set UT
		df.setTimeZone(getUT());
		
		// Get this DateFormat's calendar.
		Calendar c = df.getCalendar();
		
		// set the time.
		c.setTime(d);
		
		yrStr = new String ("" + (c.get (Calendar.YEAR) - 1900));
		int month  = c.get (Calendar.MONTH);
		if (month < 10)
		monthStr = new String ( "0" + month);
		else
		monthStr = new String ("" + month); 
		
		int datenum = c.get (Calendar.DAY_OF_MONTH);
		if (datenum < 10) 
		dateStr = new String ("0" + datenum);
		else 
		dateStr = new String ("" + datenum);

		return (df.format (d));
		*/
		/* 
		if (separator != null)
		return mo + separator + dt + separator + yr;
		else
		return mo + dt + yr;
		*/
	}

	static public Date parseYYMMDD(String s) {
		int yr, mo, dt;
		Date d;
		
		yr = Integer.parseInt(s.substring(0,2));
		mo = Integer.parseInt(s.substring(2,4));
		dt = Integer.parseInt(s.substring(4,6));
		
		if (yr < 97) yr += 100;    // set 1997 as the roll-over year

		Calendar c = Calendar.getInstance ();
		c.set (Calendar.YEAR, yr);
		c.set (Calendar.MONTH, mo -1);
		c.set (Calendar.DAY_OF_MONTH, dt);
		d = c.getTime();
		return d;
	}
	/**
	 * Converts a date to the UARC standard formatted date string.
	 * @param date The date to format.
	 * @return A string representation of the date.
	 *
	 */
	public static String dateToString(long date) {
		// Get a Date object for the given date.
		Date d = new Date(date);
		
		// Get a nifty date formatter.  This handles the "20-May-74" type thing.
		DateFormat df = DateFormat.getDateInstance(DateFormat.MEDIUM);
		
		// set UT
		df.setTimeZone(getUT());
		
		// Get this DateFormat's calendar.
		Calendar c = df.getCalendar();
		
		// set the time.
		c.setTime(d);
		
		// get the time stuff.  This is why we hack...to get the 24 hour clock
		int hour		= c.get(Calendar.HOUR_OF_DAY);
		int minute	= c.get(Calendar.MINUTE);
		int second	= c.get(Calendar.SECOND);
		
		String minuteString;
		if (minute < 10) {
			minuteString = new String("0"+minute);
		} else {
			minuteString = Integer.toString(minute);
		}
		
		String secondString = null;
		if (second < 10) {
			secondString = new String("0"+second);
		} else {
			secondString = Integer.toString(second);
		}
		
		return (df.format(d)+" "+hour+":"+minuteString+":"+secondString+" UT");
	}
	
	
	/**
	 * Returns the Frame that the Component is in.
	 * @param comp The Component to find a frame for.
	 * @return The Frame that the given Component is in.
	 *
	 */
	public static Frame getFrame(Component comp) {
		Component ret = comp;
		while (!(ret instanceof Frame)) {
			ret = ret.getParent();
		}
		return (Frame) ret;
	}
	
	/**
	 * Creates and returns a TimeZone Representing UT.
	 * @return A TimeZone representing UT.
	 *
	 */
	public static TimeZone getUT() {
		return new SimpleTimeZone(0, "UT");
	}
	
	
	/**
	 * Returns an instance of the requested object.
	 * Lumps all exceptions.
	 * @param className The name of the class to instantiate.
	 * @param parameters An array of the parameters.
	 * @exception java.lang.InstantiationException if any Exceptions are caught.
	 *
	 */
	public static Object instantiate(String className, 
		Object parameters[]) 
		throws InstantiationException {
		String err = null;
		try {
			Class theClass = Class.forName(className);
			if (parameters == null) {
				return theClass.newInstance();
			} else {
				Class parameterTypes[] = new Class[parameters.length];
				Object dumbobj = new Object();
				for (int x = 0; x < parameters.length; x++) {
					// if passing a null parameter, make it of Object type
					if (parameters[x] == null) {
						parameters[x] = dumbobj;
						parameterTypes[x] = dumbobj.getClass();
					}
					else
						parameterTypes[x] = parameters[x].getClass();
				}
				Constructor c = null;
				try {
					c = theClass.getConstructor(parameterTypes);
				} catch (NoSuchMethodException ex) {
					Constructor[] constructors = theClass.getConstructors();
					for (int x = 0; x < constructors.length; x++) {
						Class cpts[] = constructors[x].getParameterTypes();
						if (cpts.length != parameterTypes.length) 
							continue;
						int y = 0;
						for (y = 0; y < cpts.length; y++) {
							if (!(cpts[y].isAssignableFrom(parameterTypes[y]))) {
								// check for wrappers
								if (cpts[y].isPrimitive()) {
									if ((cpts[y] == Boolean.TYPE) &&
										 (parameterTypes[y] == 
										  Class.forName("java.lang.Boolean"))) {
										continue;
									} else if ((cpts[y] == Character.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Character"))) {
										continue;
									} else if ((cpts[y] == Byte.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Byte"))) {
										continue;
									} else if ((cpts[y] == Short.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Short"))) {
										continue;
									} else if ((cpts[y] == Integer.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Integer"))) {
										continue;
									} else if ((cpts[y] == Long.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Long"))) {
										continue;
									} else if ((cpts[y] == Float.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Double"))) {
										continue;
									} else if ((cpts[y] == Double.TYPE) &&
												  (parameterTypes[y] == 
													Class.forName("java.lang.Float"))) {
										continue;
									}
								}
								break;
							}
						}
						if (y == cpts.length) {
							c = constructors[x];
							break;
						}
					}
					if (c == null) {
						throw new InstantiationException
							("Matching Constructor not Found");
					}
				}
				
				return c.newInstance(parameters);
			}
		} catch (ClassNotFoundException ex) {
			err = new String("Error finding Class "+ex);
		} catch (IllegalAccessException ex) {
			  err = new String("Initializer Inaccessible "+ex);
		} catch (IllegalArgumentException ex) {
			  err = new String("Matching Constructor not found "+ex);
		} catch (InvocationTargetException ex) {
			  err = new String("Constructor threw an exception "+ex);
			  ex.printStackTrace();
		}
		if (err != null)
			throw new InstantiationException(err);
		return null;
	}
	
	
	/**
	 * Returns an instance of the requested object.
	 * Uses the null Constructor.
	 * Lumps all exceptions.
	 * @param className The name of the class to instantiate.
	 * @exception java.lang.InstantiationException if any Exceptions are caught.
	 *
	 */
	public static Object instantiate(String className) 
		throws InstantiationException {
		return instantiate(className, null);
	}
	
	/**
	 *  Returns browser name if returned by Java, else null. For netscape 
	 *  it appears to return "Netscape Communicator"
	 */
	
	public static String getBrowser () {
		String browser = System.getProperty ("browser");
		return browser;
	}

	/** 
	 * ready: Does a sleep and then checks if in stream has 
	 * anything to read.
	 * Putting this in Util - coz its a real pain otherwise to
	 * write in the sleep to avoid blocking in every call to 
	 * readLine.
	 * @param in Input reader.
	 * @param trytime number of seconds to try, 0 if indefinite.
	 * @return true/false.
	 */
	
	public static boolean retry = false;

	
	public static synchronized boolean ready(BufferedReader in, 
						 int trytime) {

	    int trynum = trytime*5; // *5 coz we sleep 200 ms, and 
	    // the input is in seconds.

	    if (trynum == 0) {
		trynum = 10;
	    }

		int i = 0;
		Frame tmpf = new Frame ();
		String[] buttonStrs = {"Try Again", "Give Up"};

		ActionListener al = new ActionListener () {
			public void actionPerformed (ActionEvent e) {
			String label = e.getActionCommand ();
			if (label.equals ("Try Again")) {
			IPMAStandard.retry = true;
			}
			else if (label.equals ("Give up")) {
			IPMAStandard.retry = false;
			}
			}
			};

		while (true) {
			try {
				while (! in.ready()) {
					try {
						Thread.sleep (200);
					} catch (InterruptedException e) {}
					if (i++ > trynum) break;
				}
			} catch (IOException e) {
				Debug.print (Debug.ERR, "IPMAStandard: ready: IO Exception");
			}
			
			if (i > trynum) { // still not ready
				// see if you need to try again 
//				GeneralDialog gd = new GeneralDialog(tmpf,
//					"Read timed out", buttonStrs, al);
			    if (trytime > 0) {// try limited time, return
				return false;
			    }

				new GeneralDialog(tmpf,
						"Read timed out", buttonStrs, al);
				if (retry) {
					// try for another 2 seconds.
					i = 0;
					retry = false;
					continue;
				}
				else // dont retry
					return false;
			}
			else {
				// got data , it is safe to read line.
				return true;
			}
		}
	}
	
}
