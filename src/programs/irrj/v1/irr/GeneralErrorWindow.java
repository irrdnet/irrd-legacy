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

import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.net.*;
import java.io.*;
import ipma.Window.*;

/*
  A general window with a noneditable text area for displaying errors
  By Jon Poland
  March 15, 1999
*/

public class GeneralErrorWindow extends CPopupWindow{

    TextArea ta;
    protected ActionListener al = new InnerActionAdapter();
 
    public GeneralErrorWindow(){
	super.setTitle("Status");
	ta = new TextArea(40, 60);
	ta.setEditable(false);
	BuildLayout();
	pack();
	show();
    }

    public GeneralErrorWindow(String error){
	super.setTitle("Status");
	ta = new TextArea(40, 60);
	ta.setEditable(false);
	ta.setText(error.trim());
	BuildLayout();
	pack();
	show();
    }
    
    public GeneralErrorWindow(String error, int width, int height){
        super.setTitle("Status");
	ta = new TextArea(width, height);
	ta.setEditable(false);
	ta.setText(error.trim());
	BuildLayout();
	pack();
	show();
    }
    public void appendText(String t){
	ta.append(t.trim());
    }

    public TextArea getArea(){
	return ta;
    }

    public void clearText(){
	ta.setText("");
    }
    protected void BuildLayout(){
        setLayout(new BorderLayout());

	add("Center", ta);
	
	Panel p = new Panel();
	Button b = new Button("Close");
	b.addActionListener(al);
	p.add(b);
	
	add("South", p);
    }
    
    protected void closeWindow(){
       super.Close();
       dispose();
    }

    class InnerActionAdapter implements ActionListener {
	public void actionPerformed (ActionEvent e) {
	    String label = e.getActionCommand ();
	    /* if Close button */
	    if(label.equals("Close"))
	      closeWindow();
	}
    }
}
