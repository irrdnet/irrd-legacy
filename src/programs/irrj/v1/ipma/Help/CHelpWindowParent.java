package ipma.Help;



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

 * CHelpWindowParent

 * 

 * A CHelpWindowParent contains the text for a certain object's help

 * windows as well as a reference to every CHelpWindow pertaining 

 * to that object.

 * 

 * Modification History:

 *

 * @version 1.0

 * @date 

 * @author Jimmy Wan

 */





import java.util.*;



public class CHelpWindowParent {

	

	String   HelpTitle;

	String   HelpText[][];

	

	Vector   HelpWindows;

	

	boolean  MultiPane;

	

	/** CHelpWindowParent: Constructor for a parent of single page 

	 *  help windows.

	 *  @param HelpTitle Title for Help Window.

	 *  @param HelpText Text to be displayed.

	 */





	public CHelpWindowParent(String HelpTitle, String HelpText[]) {

		this.HelpTitle   = HelpTitle;

		this.HelpText    = new String[1][];

		this.HelpText[0] = HelpText;

		HelpWindows      = new Vector();

		MultiPane = false;

	}





	/** CHelpWindowParent: Constructor for a parent of multiple 

	 *  page help windows.

	 *  @param HelpTitle Title for Help Window.

	 *  @param HelpText Text to be displayed in window. (dual array).

	 */

	

	public CHelpWindowParent(String HelpTitle, String HelpText[][]) {

		this.HelpTitle = HelpTitle;

		this.HelpText  = HelpText;

		HelpWindows    = new Vector();

		MultiPane = true;

	}

	



	/** ShowHelpWindow: Create a new help window and display it.

	 */

	public void ShowHelpWindow() {

		

		if (MultiPane)

			HelpWindows.addElement(new CHelpWindow(this, HelpTitle, HelpText));

		else

			HelpWindows.addElement(

				new CHelpWindow(this, HelpTitle, HelpText[0]));

	}



	

	/** KillHelpWindows: Hide and destroy all help windows 

	 *  belonging to this CHelpWindowParent.

	 */



	public final void KillHelpWindows() {

		CHelpWindow HelpWindow;

		

		while (HelpWindows.size() > 0) {

			HelpWindow = (CHelpWindow) HelpWindows.firstElement();

			HelpWindows.removeElementAt(0);

			HelpWindow.Close();

		}

	}

	

	/** RemoveHelpWindowReference: Remove the reference to the 

	 *  given CHelpWindow.

	 *  @param HelpWindow The window in question.

	 */

	public final void RemoveHelpWindowReference(CHelpWindow HelpWindow) {

		HelpWindows.removeElement(HelpWindow);

	}



}

