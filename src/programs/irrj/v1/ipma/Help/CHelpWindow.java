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

 * CHelpWindow

 * 

 * CHelpWindow is a help window that is attached to a single

 * CHelpWindowParent.  CHelpWindows can be either a single page or

 * multiple pages.

 * 

 * Modification History:
 * $Log: CHelpWindow.java,v $
 * Revision 1.1  2000/08/19 14:35:48  polandj
 * Sorry for all the emails this is going to generate...
 *    -  Moved old IRRj into irrj/v1
 *    -  New IRRj is in irrj/v2
 *
 * Revision 1.2  2000/03/01 21:28:04  polandj
 * Updating to the latest version.
 *
 * Revision 1.1  1999/05/14 19:39:06  polandj
 * Adding irrj under repository
 *
 * Revision 1.7  1999/03/09 21:52:52  vecna
 * Changed all instances of "show()" to "setVisible(true)".
 * Removed all incorrect sizing statements.  getPreferredSize doesn't
 * really work as intended...
 *:

 */





import java.awt.*;

import java.awt.event.*;



import ipma.Window.*;



public class CHelpWindow extends CPopupWindow {

	

	public static final String PAGE_STRING = "Page ";

	public static final String OF_STRING   = " of ";

	public static final String NEXT_STRING = "Next";

	public static final String PREV_STRING = "Prev";

	

	public static final Color MAIN_FCOLOR = Color.black;

	public static final Color MAIN_BCOLOR = Color.white;

	public static final Color BUTTON_FCOLOR = Color.black;

	public static final Color BUTTON_BCOLOR = Color.lightGray;

	

	CHelpWindowParent Parent;

	

	String   HelpText[][];

	Panel    MainHelpPanel;

	Panel    HelpTextPanel;

	Panel    ButtonPanel;

	Button   NextButton;

	Button   PrevButton;

	Label    PageLabel;

	int      CurrentPage;

	boolean  MultiplePanes;

	

	/** CHelpWindow: Constructor.

	 *  @param Parent.

	 */



	public  CHelpWindow(CHelpWindowParent Parent) {

		this.Parent = Parent;

	}

	

	/** CHelpWindow: Constructor for a single page help window.

	 *  @param Parent.

	 *  @param HelpTitle Title for window.

	 *  @param HelpText Text to be put in the window.

	 */



	public  CHelpWindow(CHelpWindowParent Parent,

		String HelpTitle, String HelpText[]) {

		this.Parent = Parent;
		Setup(HelpTitle, HelpText);
		setVisible (true);
	}

	

	

	/** CHelpWindow: Constructor for a multiple page help window.

	 *  @param Parent.

	 *  @param HelpTitle Title for Help window.

	 *  @param HelpText Text to be displayed.

	 */



	public  CHelpWindow(CHelpWindowParent Parent,

		String HelpTitle, String HelpText[][]) {

		this.Parent = Parent;
		Setup(HelpTitle, HelpText);
		setVisible(true);
	}

	



	public final void validate() {

		HelpTextPanel.validate();

		super.validate();

	}



	/** setBounds: override

	 *  @param x X value for top left corner.

	 *  @param y Y value for top left corner.

	 *  @param w  width.

	 *  @param h  height.

	 */



	public final void setBounds(int x, int y, int w, int h) {

		super.setBounds(x, y, w, h);

		validate();

	}  



	/** Setup: Sets up a single page GUI for showing help information.

	 *  @param HelpTitle Title for window.

	 *  @param HelpText Text to be displayed.

	 */



	public void Setup(String HelpTitle, String HelpText[]) {

		

		this.HelpText    = new String[1][];

		this.HelpText[0] = HelpText;

		

		BuildSinglePaneLayout();

		super.Setup(HelpTitle, MainHelpPanel);

	}



	/** Setup: Sets up a multiple page GUI for showing help info.

	 *  @param HelpTitle Title for window.

	 *  @param HelpText Text to be displayed (dual array).

	 */

	

	public void Setup(String HelpTitle, String HelpText[][]) {

		

		this.HelpText = HelpText;

		

		BuildMultiPaneLayout();

		super.Setup(HelpTitle, MainHelpPanel);

	}

	

	

	/** NextPage: Move to the next page if available and build it.

	 */



	public final void NextPage() {

		

		if (!MultiplePanes)

			return;

		if (CurrentPage == HelpText.length - 1)

			return;



		CurrentPage++;

		BuildPage();

	}

	

	/**

	 * Move to the previous page if available and build it.

	 */



	public final void PrevPage() {

		

		if (!MultiplePanes)

			return;

		if (CurrentPage == 0)

			return;

		

		CurrentPage--;

		BuildPage();

	}

	

	

	/** BuildSinglePanelLayout:Builds a single page help window.

	 */



	protected final void BuildSinglePaneLayout() {

		

		Label HelpLabel;

		

		MultiplePanes = false;

		HelpTextPanel = new Panel();

		HelpTextPanel.setLayout(new GridLayout(HelpText[0].length, 1));

		HelpTextPanel.setForeground(MAIN_FCOLOR);

		HelpTextPanel.setBackground(MAIN_BCOLOR);

		

		int   i = 0;

		do {

			HelpLabel = new Label(HelpText[0][i]);

			HelpTextPanel.add(HelpLabel);

		} while (++i < HelpText[0].length);

		

		MainHelpPanel = new Panel(); 

		MainHelpPanel.setLayout(new BorderLayout());

		MainHelpPanel.add("Center", HelpTextPanel);

		MainHelpPanel.setForeground(MAIN_FCOLOR);

		MainHelpPanel.setBackground(MAIN_BCOLOR);

		setForeground(MAIN_FCOLOR);

		setBackground(MAIN_BCOLOR);

		repaint();

	}

	

	

	/** BuildPage:Builds a single page in a multiple page help window.

	 */



	protected final void BuildPage() {

		Label HelpLabel;

		

		HelpTextPanel.removeAll();   

		HelpTextPanel.setLayout(

			new GridLayout(HelpText[CurrentPage].length, 1));

		int   i = 0;

		do {

			HelpLabel = new Label(HelpText[CurrentPage][i]);

			HelpTextPanel.add(HelpLabel);

		} while (++i < HelpText[CurrentPage].length);

		PageLabel.setText(PAGE_STRING + Integer.toString(CurrentPage + 1) +

								OF_STRING + Integer.toString(HelpText.length));

		ButtonPanel.invalidate();

		HelpTextPanel.invalidate();

		MainHelpPanel.invalidate();

		validate();

		repaint();

	}

	

	

	/** BuildMultiPanelLayout: Builds a Multiple Page help window.

	 */



	protected final void BuildMultiPaneLayout() {

		

		MultiplePanes = true;

		HelpTextPanel = new Panel();

		HelpTextPanel.setForeground(MAIN_FCOLOR);

		HelpTextPanel.setBackground(MAIN_BCOLOR);

		

		NextButton = new Button(NEXT_STRING);

		PrevButton = new Button(PREV_STRING);



		PageLabel  = new Label();



		// Create listener for Next and Previous buttons...

		InnerActionListener al = new InnerActionListener();



		NextButton.addActionListener(al);

		PrevButton.addActionListener(al);

		

		NextButton.setForeground(BUTTON_FCOLOR);

		NextButton.setBackground(BUTTON_BCOLOR);

		PrevButton.setForeground(BUTTON_FCOLOR);

		PrevButton.setBackground(BUTTON_BCOLOR);

		PageLabel.setForeground(MAIN_FCOLOR);

		PageLabel.setBackground(MAIN_BCOLOR);

		

		ButtonPanel = new Panel();

		ButtonPanel.setLayout(new GridLayout(1, 3));

		ButtonPanel.add(PrevButton);

		ButtonPanel.add(PageLabel);

		ButtonPanel.add(NextButton);

		ButtonPanel.setForeground(MAIN_FCOLOR);

		ButtonPanel.setBackground(MAIN_BCOLOR);

		

		MainHelpPanel = new Panel();

		MainHelpPanel.setLayout(new BorderLayout());

		MainHelpPanel.add("Center", HelpTextPanel);

		MainHelpPanel.add("South", ButtonPanel);

		MainHelpPanel.setForeground(MAIN_FCOLOR);

		MainHelpPanel.setBackground(MAIN_BCOLOR);

		setForeground(MAIN_FCOLOR);

		setBackground(MAIN_BCOLOR);

		CurrentPage = 0;

		BuildPage();

	}

	

	

	/** Close: Close the help window and remove its reference from 

	 *  the CHelpWindowParent.

	 */



	public void  Close() {

		Parent.RemoveHelpWindowReference(this);

		super.Close();

	}



	// Inner class to handle actions from buttons.

	class InnerActionListener implements ActionListener {

		public void actionPerformed(ActionEvent e) {



			String label = e.getActionCommand();

			if (label.equals(NEXT_STRING)) {

				NextPage ();

			}

			else if (label.equals(PREV_STRING)) {

				PrevPage();

			}

		}

	}

}

