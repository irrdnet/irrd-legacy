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

 * CErrorWindow

 * 

 * Displays a window showing the error that occurred and 

 * some contact info.

 * 

 * Modification History:

 * Original code written in java 1.0.2. This version is a port

 * to jdk 1.1 

 * @version 1.1.

 * @date 9 June 1998.

 * @author Sushila Subramanian.

 *

 * @version 1.0

 * @date 20 April 1998

 * @author Jimmy Wan

 */



import java.awt.*;

import java.awt.event.*;

import java.util.*;



import ipma.Util.*;

import ipma.Window.*;



public class CErrorWindow extends CPopupWindow {

	

	static final int HELP_TEXT_ROWS = 4;

	static final int HELP_TEXT_COLS = 55;

	

	static final int SMTP_PORT    = 25;

	

	static final String TARGET_ADDRESS  = "ipma-error@merit.edu";

	static final String DEFAULT_USER    = "IPMAToolUser@merit.edu";

	

	static final String TITLE  = "Error";

	

	static final String EXCEPTION_TEXT[] = {

		"A Java Exception occurred"

	};

	

	static final String SECURITY_EXCEPTION_TEXT[] = {

		"A Java Security Exception occurred\n",

		"This is most likely caused by a firewall restricting\n",

		"access to the ports needed by the Salamander\n",

		"Server, or by rejecting our request to open a socket."

	};

	

	static final String JAVA_PROPERTY_TEXT =

														 "Your WWW Browser of Java Virtual Machine would\n" +

														 "not allow access to one or all of the System\n" +

														 "properties.";

	

	static final String CONTACT_INFO =

												 "Please try clearing your cache and restarting\n" +

												 "your browser.  If you are still unable to connect,\n" +

												 "please email the contents of your Java Console\n" +

												 "as well as the contents in these windoes\n" +

												 "ipma-support@merit.edu for further assistance\n";

	

	static final String EMAIL_STRING       = "Email:";

	static final String SEND_STRING        = "Send Email";

	static final String SEND_ANON_STRING   = "Send Anonymous Email";

	

	static final String FONT      = "Courier";

	static final int    FONT_SIZE = 11;

	protected static final int DEFAULT_WIDTH  = 480;

	protected static final int DEFAULT_HEIGHT = 360;

	

	public String      Server;

	public Button      SendButton;

	public Button      SendAnonButton;

	public TextField   EmailField;

	public TextArea    HelpTextArea;

	public TextArea    JavaInfoArea;

	public TextArea    ContactTextArea;

	

	/** CErrorWindow: Constructor.

	 *  @param e Exception.

	 *  @param Server.

	 */



	public  CErrorWindow(Exception e, String Server) {

		this.Server = Server;

		BuildLayout(e, DEFAULT_WIDTH, DEFAULT_HEIGHT);

	}

	

	/** CErrorWindow: Constructor with width and height thrown in.

	 *  @param e Exception to use to determine what message to print.

	 *  @param Server.

	 *  @param width.

	 *  @param height.

	 */ 

	

	public  CErrorWindow(Exception e, String Server, int width, int height) {

		this.Server = Server;

		BuildLayout(e, width, height);

	}

	

	/** BuildLayout: Builds layout based on gridbag layout.

	 *  @param e  Exception to use to determine which message to print.

	 *  @param width.

	 *  @param height.

	 */

	

	private final void BuildLayout(Exception e, int width, int height) {



		GridBagLayout        layout;

		GridBagConstraints   c;



		String      HelpText[];

		

		Panel       HelpPanel;

		int         i = 0;

		

		Font     font = new Font(FONT, Font.PLAIN, FONT_SIZE);

		

		if (e instanceof SecurityException)

			HelpText = SECURITY_EXCEPTION_TEXT;

		else

			HelpText = EXCEPTION_TEXT;

		

		HelpTextArea = new TextArea(HELP_TEXT_ROWS, HELP_TEXT_COLS);

		HelpTextArea.setEditable(false);

		HelpTextArea.setFont(font);

		do {

			//1.1 port

			//      HelpTextArea.appendText(HelpText[i]);

			HelpTextArea.append (HelpText[i]);

		} while (++i < HelpText.length);



		// 1.1 port

		//    HelpTextArea.appendText('\n' + e.toString());

		HelpTextArea.append('\n' + e.toString());

		

		JavaInfoArea = new TextArea(HELP_TEXT_ROWS, HELP_TEXT_COLS);

		JavaInfoArea.setEditable(false);

		JavaInfoArea.setFont(font);

		

		try {

			Properties  SystemProperties;

			Enumeration PropertiesEnum;

			String      Property;

			

			SystemProperties = System.getProperties();

			PropertiesEnum   = SystemProperties.propertyNames();

			

			while (PropertiesEnum.hasMoreElements()) {

				Property = (String)PropertiesEnum.nextElement();

				JavaInfoArea.append(

					Property + ":\n\t" + 

					SystemProperties.getProperty(Property) + '\n');

			}

		}

		catch (SecurityException SE) {

			JavaInfoArea.append(JAVA_PROPERTY_TEXT);

		}

		

		ContactTextArea = new TextArea(HELP_TEXT_ROWS, HELP_TEXT_COLS);

		ContactTextArea.setEditable(false);

		ContactTextArea.setFont(font);

		

		ContactTextArea.append(CONTACT_INFO);

		

		Label       EmailLabel;

		EmailLabel = new Label(EMAIL_STRING);

		EmailLabel.setFont(font);

		EmailField = new TextField();

		EmailField.setFont(font);

		

		// add inner class that will handle button actions.



		// an actionListener for Buttons.

		ActionListener al = new ActionListener() {

			public void actionPerformed(ActionEvent e) {

			String label = e.getActionCommand();

			if (label.equals (SEND_STRING)) {

			SendMail Mail  = new SendMail();

			Mail.setSMTPServer(Server);

			Mail.setTo(TARGET_ADDRESS);

			Mail.setFrom(EmailField.getText());

			Mail.setBody(HelpTextArea.getText() +

			ContactTextArea.getText() +

			JavaInfoArea.getText());

			}

			else if (label.equals (SEND_ANON_STRING)) {

			SendMail Mail  = new SendMail();

			Mail.setSMTPServer(Server);

			Mail.setTo(TARGET_ADDRESS);

			Mail.setFrom(DEFAULT_USER);

			Mail.setBody(HelpTextArea.getText() +

			ContactTextArea.getText() +

			JavaInfoArea.getText());

			}



			}

			};



		SendButton = new Button(SEND_STRING);

		SendButton.addActionListener (al);

		SendButton.setFont(font);

		SendAnonButton = new Button(SEND_ANON_STRING);

		SendAnonButton.addActionListener (al);

		SendAnonButton.setFont(font);

		

		layout = new GridBagLayout();

		HelpPanel = new Panel();

		c = new GridBagConstraints();

		c.fill         = GridBagConstraints.BOTH;

		c.gridwidth    = GridBagConstraints.RELATIVE;

		c.weightx      = 2.0;

		c.weighty      = 5.0;

		layout.setConstraints(HelpTextArea, c);

		

		c.gridwidth    = GridBagConstraints.REMAINDER;

		layout.setConstraints(ContactTextArea, c);



		c.weightx      = 8.0;

		c.weighty      = 10.0;

		layout.setConstraints(JavaInfoArea, c);

		

		c.fill         = GridBagConstraints.NONE;

		c.gridwidth    = GridBagConstraints.RELATIVE;

		c.gridheight   = GridBagConstraints.RELATIVE;

		c.weightx      = 0;

		c.weighty      = 0;

		layout.setConstraints(EmailLabel, c);

		

		c.fill         = GridBagConstraints.HORIZONTAL;

		c.gridwidth    = GridBagConstraints.REMAINDER;

		layout.setConstraints(EmailField, c);

		

		c.fill         = GridBagConstraints.NONE;

		c.gridwidth    = GridBagConstraints.RELATIVE;

		c.gridheight   = GridBagConstraints.REMAINDER;

		c.ipadx        = 2;

		c.ipady        = 2;

		layout.setConstraints(SendButton, c);

		

		c.gridwidth    = GridBagConstraints.REMAINDER;

		layout.setConstraints(SendAnonButton, c);

		



		HelpPanel.setLayout(layout);

		HelpPanel.add(HelpTextArea);

		HelpPanel.add(ContactTextArea);

		HelpPanel.add(JavaInfoArea);

		HelpPanel.add(EmailLabel);

		HelpPanel.add(EmailField);

		HelpPanel.add(SendButton);

		HelpPanel.add(SendAnonButton);

		

		Setup(TITLE, HelpPanel, width, height);

		Dimension   ScreenSize;

		int   x = 0,

			y = 0;



		ScreenSize = getToolkit().getScreenSize();

		if (ScreenSize.width > width)

			x = ScreenSize.width - width;

		if (ScreenSize.height > height)

			y = ScreenSize.height - height;

		// 1.1 port

		//    move(x, y);

		setLocation (x, y);

		setVisible (true);

	}  

	



	/** Close: Clean up before closing an application.

	 */

	

	public void Close() {

		super.Close();

		SendButton        = null;

		SendAnonButton    = null;

		EmailField        = null;

		HelpTextArea      = null;

		JavaInfoArea      = null;

		ContactTextArea   = null;

	}

	

	

}

