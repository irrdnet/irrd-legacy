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


import java.awt.*;
import java.awt.event.*;
import java.util.*;

import ipma.Help.*;
import ipma.PlugIn.*;
import ipma.Query.*;
import ipma.Salamander.*;
import ipma.Tools.*;
import ipma.Wait.*;
import ipma.Window.*;

import debug.Debug;

// creates a menuItem that will uniformly handle modifications to
// preferences. the main thing here is we need common code, so this
// menu item can simply be added to tools, and the action handler 
// for this item is in common code.

public class PrefMenuItemGUI {
   // bunch of defines for Button strings
   
   static final String OKB = "OK";
   static final String CANCELB = "Cancel";
   static final String SAVEB = "Save";
   static final String CONFIGB = "Show Saved Config";
   static final String CLOSECONFIGB = "Close Config Display";

   // more defines for Strings at the top level
   static final String COMMON = "IPMA";
   static final String FG = "FlapGraph";
   static final String FT = "FlapTable";
   static final String AS = "ASExplorer";


   // GUI related objects
   protected PreferenceManager prefs;
   protected Frame prefFrame, choiceFrame;
   protected Panel topPanel, middlePanel, bottomPanel;
   protected Panel bottomButtonPanel, bottomTextPanel;
   protected TextArea bottomText;
   protected Panel prefLabelPanel, prefChoicePanel;
   protected Button cancelB, okB, saveB, showChoiceB, closeConfigB;
   protected TextArea choiceText;
   protected PopupMenu pmenu; // just used to pop up a string.

   // stick one vector for Strings for each tool, and 
   // one for "IPMA" which is the common values - like base URL.
   protected Vector CommonVec;
   protected Vector FlapGraphVec;
   protected Vector ASExplorerVec;
   protected Vector FlapTableVec;

   public ActionListener al = new InnerActionAdapter();
   public ItemListener il = new InnerItemAdapter ();
   public MouseMotionListener mml = new InnerMouseMotionAdapter();
   protected Label valueLabel;
   protected boolean stateChange = false; // used to determine if 
   // anything needs to be saved.

   // needed to determine available choices.
   
   protected CSalamanderImpl           Saly;
   protected CSalamanderListener   SalyListener;
   protected ResponseHandler       SalyHandler;
   protected QuerySubmitter        querySubmitter;
   

   public PrefMenuItemGUI() {
      prefs = PreferenceManager.getInstance();
      valueLabel = new Label ("Values: ");

      // initialize vectors

      CommonVec = new Vector (5,3);
      FlapGraphVec = new Vector (5, 3);
      FlapTableVec = new Vector (5, 3);
      ASExplorerVec = new Vector (5, 3);

      // sort all the info in the preference file
      sortAllInfo();

      // if the tools are not up - need to make salamander
      // query here to figure out all possible choices.
      setUpChoices ();

      // now build GUI to allow mods.
      buildBaseGUI();
      prefFrame.show();
   }

   private void sortAllInfo () throws IllegalArgumentException {
      String key, value, subkey;
      Enumeration e = prefs.prefs.propertyNames();
      while (e.hasMoreElements()) {
         key = (String) e.nextElement();
         value = (String) prefs.prefs.get (key);

         if (value == null) return;

			if (key.indexOf(".") == -1 ){

			  subkey = key;
			}

			else{
			  try {
				 subkey = key.substring (0, key.indexOf ("."));
			  }
			  catch (Exception err) {
				 Debug.print (Debug.ERR, "Error in ipmarc file format");
				 throw new IllegalArgumentException ("Error in .ipmarc");
			  }
			}

         if (subkey.equals (COMMON)){
            CommonVec.addElement (key);
         }
         else if (subkey.equals (FG)) {
            FlapGraphVec.addElement (key);
         }
         else if (subkey.equals (FT)) {
            FlapTableVec.addElement (key);
         }
         else if (subkey.equals (AS)) {
            ASExplorerVec.addElement (key);
         }

      }
   }


   private void buildBaseGUI () {

      Button tmpB;

      // outer frame
      prefFrame = new Frame ("Configure Preferences");
      prefFrame.setLayout (new BorderLayout());

      // top panel - contains buttons - one for each tool, 
      // and one for common IPMA options.
      
      topPanel = new Panel ();
      topPanel.setLayout (new FlowLayout ());

      topPanel.add (new Label ("Click Module to be Modified"));
      // add buttons to the top level only if there is a
      // defined option in the .ipmarc file, else there's
      // nothing to modify.


      if (CommonVec.size () > 0) {
         tmpB = new Button (COMMON);
         topPanel.add (tmpB);
         tmpB.addActionListener (al);
      }

      if (FlapGraphVec.size() > 0) {
         tmpB = new Button (FG);
         topPanel.add (tmpB);
         tmpB.addActionListener (al);
      }

      if (FlapTableVec.size() > 0) {
         tmpB = new Button (FT);
         topPanel.add (tmpB);
         tmpB.addActionListener (al);
      }

      if (ASExplorerVec.size() > 0) {
         tmpB = new Button (AS);
         topPanel.add (tmpB);
         tmpB.addActionListener (al);
      }

      // middle panel contains the config params on left,
      // and choices on the right with the current choice
      // being item 0. the contents of this panel changes
      // based on whats picked in the top panel.

      middlePanel = new Panel();
      middlePanel.setLayout (new BorderLayout());

      prefLabelPanel = new Panel ();
      prefLabelPanel.setLayout (new GridLayout (0,1));

      prefChoicePanel = new Panel ();
      prefChoicePanel.setLayout (new GridLayout (0,1));

      middlePanel.add ("Center", prefChoicePanel);
      middlePanel.add ("West", prefLabelPanel);
      
      middlePanel.setBackground (Color.black);
      middlePanel.setForeground (Color.white);
      // bottom panel just contains buttons 

      cancelB = new Button (CANCELB);
      okB = new Button (OKB);
      saveB = new Button (SAVEB);
      showChoiceB = new Button (CONFIGB);
      okB.setEnabled (false);
      saveB.setEnabled (false);

      cancelB.addActionListener (al);
      cancelB.addMouseMotionListener (mml);
      okB.addActionListener (al);
      okB.addMouseMotionListener (mml);
      saveB.addActionListener (al);
      saveB.addMouseMotionListener (mml);
      showChoiceB.addActionListener (al);
      showChoiceB.addMouseMotionListener (mml);

      bottomPanel = new Panel();
      bottomPanel.setLayout (new GridLayout(0, 1));

      bottomButtonPanel = new Panel();
      bottomButtonPanel.setLayout (new FlowLayout());

      bottomButtonPanel.add (okB);
      bottomButtonPanel.add (cancelB);
      bottomButtonPanel.add (saveB);
      bottomButtonPanel.add (showChoiceB);
      bottomButtonPanel.addMouseMotionListener (mml);

      bottomText = new TextArea (" ", 2, 50, TextArea.SCROLLBARS_NONE);
      bottomText.setEditable (false);
      bottomTextPanel = new Panel();
      bottomTextPanel.setLayout (new FlowLayout());
      bottomTextPanel.add (bottomText);

      bottomPanel.add (bottomButtonPanel);
      bottomPanel.add (bottomTextPanel);

      

      // add the two panels to the outer frame.

      prefFrame.add ("North", topPanel);
      prefFrame.add ("Center", middlePanel);
      prefFrame.add ("South", bottomPanel);
      prefFrame.pack ();
      prefFrame.setVisible (true);

      // create the choice frame but dont show it 
      // unless necessary.
      choiceFrame = new Frame ("Currently Saved Configuration");
      choiceText = new TextArea ();
      choiceText.setEditable (false);

      closeConfigB = new Button (CLOSECONFIGB);
      closeConfigB.addActionListener (al);
      choiceFrame.add ("Center", choiceText);
      choiceFrame.add ("South", closeConfigB);

      choiceFrame.setVisible (false);	
      
   }

   private void setUpChoices () {
      Query                 ChannelQuery;
      
      Saly = StartApps.Saly;
      SalyHandler = StartApps.SalyHandler;
      SalyListener = StartApps.SalyListener;
      querySubmitter = StartApps.querySubmitter;
      SalyHandler.RegisterTypeChecker(new CListChecker());
      
      // try FlapGraph first.
      if (flapGraph.CFlapGraphWindow.ixpNames.getItemCount() == 0) {
         
         try {
            ChannelQuery = new Query(flapGraph.FlapGraph.FGCHANNEL);
            querySubmitter.SubmitQuery(ChannelQuery);
            
            if (!SalyHandler.WaitForReady(ChannelQuery)) {
               SalyListener.DecrementNumConsumers();
               return;
            }
            
            CDataCollection Collection;
            
            Collection = ChannelQuery.getCollection();
            CListDataSet   ListDataSet =
                                        (CListDataSet) Collection.GetNewestDataSet();

            int i = ListDataSet.Channel.size();
            String ixp;
            for (int j= 0; j < i; j++) {
               ixp = (String) ListDataSet.Channel.elementAt(j);
               if (ixp.endsWith ("all")) {
                  ixp = ixp.substring(ixp.indexOf(":") + 1);
                  ixp = ixp.substring(0, ixp.lastIndexOf (":"));
                  flapGraph.CFlapGraphWindow.ixpNames.add (ixp);
               }
            }

         }
         catch (Exception e) {
            e.printStackTrace();
            if (SalyListener != null)
               SalyListener.DecrementNumConsumers();
         }
      }

      // flaptable info

      if (flapTable.CFlapTableWindow.ixpNames.getItemCount() == 0) {
         
         try {
            ChannelQuery = new Query(flapTable.FlapTable.FTBCHANNEL);
            querySubmitter.SubmitQuery(ChannelQuery);
            
            if (!SalyHandler.WaitForReady(ChannelQuery)) {
               SalyListener.DecrementNumConsumers();
               return;
            }
            
            CDataCollection Collection;
            
            Collection = ChannelQuery.getCollection();
            CListDataSet   ListDataSet =
                                        (CListDataSet) Collection.GetNewestDataSet();

            int i = ListDataSet.Channel.size();
            String ixp;
            for (int j= 0; j < i; j++) {
               ixp = (String) ListDataSet.Channel.elementAt(j);
               ixp = ixp.substring(ixp.indexOf(":") + 1);
               flapTable.CFlapTableWindow.ixpNames.add (ixp);
            }

         }
         catch (Exception e) {
            e.printStackTrace();
            if (SalyListener != null)
               SalyListener.DecrementNumConsumers();
         }
      }

      // asexplorer info
      if (asExplorer.CASExplorerFrame.ixpNames.getItemCount() == 0) {
         
         try {
            ChannelQuery = new Query(asExplorer.ASExplorer.ASCHANNEL);
            querySubmitter.SubmitQuery(ChannelQuery);
            
            if (!SalyHandler.WaitForReady(ChannelQuery)) {
               SalyListener.DecrementNumConsumers();
               return;
            }
            
            CDataCollection Collection;
            
            Collection = ChannelQuery.getCollection();
            CListDataSet   ListDataSet =
                                        (CListDataSet) Collection.GetNewestDataSet();

            int i = ListDataSet.Channel.size();
            String ixp;
            for (int j= 0; j < i; j++) {
               ixp = (String) ListDataSet.Channel.elementAt(j);
               ixp = ixp.substring(ixp.indexOf(":") + 1);
               asExplorer.CASExplorerFrame.ixpNames.add (ixp);
            }

         }
         catch (Exception e) {
            e.printStackTrace();
            if (SalyListener != null)
               SalyListener.DecrementNumConsumers();
         }
      }

      // this kills the app, but it seems like this ought to be here...
      //SalyListener.DecrementNumConsumers();
   }


   public String configStr (Vector vec) {
      int i;
      String key, val, modname, subkey;
      String str = "";
      
      for (i = 0; i < vec.size(); i++) {
         key = (String) vec.elementAt (i);
         try {
            modname = key.substring (0, key.indexOf ("."));
            subkey = key.substring (key.indexOf (".")+1);
         }
         catch (Exception err) {
            Debug.print (Debug.ERR, "Error in ipmarc file format");
            throw new IllegalArgumentException ("Error in .ipmarc");
         }
         val = prefs.get (modname, subkey);
         str = str + key + ": " + val + "\n";
      }
      
      return str;
   }


   public void displayChoices () {

      // build up the string to display in text area.
      String str = "";
      // first list common ones, then do per module.
      str = str + configStr (CommonVec);
      str = str + configStr (FlapGraphVec);
      str = str + configStr (FlapTableVec);
      str = str + configStr (ASExplorerVec);

      choiceText.setText (str);
      choiceFrame.pack();
      choiceFrame.setVisible (true);
   }


   // add an action listener for all the buttons
   class InnerActionAdapter implements ActionListener {
      public void actionPerformed (ActionEvent e) {
         
         /* actions need to be very application specific. if
         * you add a new preference, or a new tool, be sure
         * to update this!
         */
         
         Button aButton;
         
         Object source = e.getSource();
         String label = e.getActionCommand();
         
         if (source instanceof Button) {
            aButton = (Button) source;
         }
         else return;
         
         if (! ((label.equals (CONFIGB)) ||
                (label.equals (SAVEB)) ||
                (label.equals (CLOSECONFIGB)))) {
            prefChoicePanel.removeAll ();
            prefLabelPanel.removeAll ();
         }

         if (label.equals (CANCELB)) {
            // delete the frame.
            prefFrame.setVisible (false);
            prefFrame.removeAll ();
            prefFrame = null;
            
            return;
         }
         
         okB.setEnabled (true);
         saveB.setEnabled (true);
         

         if (label.equals (CONFIGB)) {
            displayChoices ();
            return;
         }

         if (label.equals (CLOSECONFIGB)) {
            // make choice frame invisible.
            choiceFrame.setVisible (false);	
            return;
         }

         if (label.equals (COMMON)) {
            
         }
         
         // flapgraph 
         if (label.equals (FG)) {
            prefLabelPanel.add (new Label ("FlapGraph Options:"));
            prefChoicePanel.add (valueLabel);
            
            int vecsize = FlapGraphVec.size ();
            
            for (int i = 0; i < vecsize; i++) {
               String anItem = (String) FlapGraphVec.elementAt (i);
               if (anItem.equals ("FlapGraph.DefaultChannel")) {
                  Choice c = flapGraph.CFlapGraphWindow.ixpNames;
                  c.addItemListener (il);
                  // find the current preference for this choice 
                  // and make it the first element.
                  String s1 = (String) prefs.get ("FlapGraph", "DefaultChannel");
                  String s2 = s1.substring (s1.indexOf(":") + 1);
                  String currentChoice = s2.substring (0, s2.indexOf (":"));
                  
                  prefLabelPanel.add (new Label ("DefaultChannel: "));
                  prefChoicePanel.add (c);
                  c.select (currentChoice);
                  c.repaint ();
               }
            }
            
            prefFrame.pack();
            prefFrame.setVisible (true);
            prefChoicePanel.repaint (100);
            return;
         }
         
         // flaptable
         if (label.equals (FT)) {
            prefLabelPanel.add (new Label ("FlapTable Options:"));
            prefChoicePanel.add (valueLabel);
            
            int vecsize = FlapTableVec.size ();
            
            for (int i = 0; i < vecsize; i++) {
               String anItem = (String) FlapTableVec.elementAt (i);
               if (anItem.equals ("FlapTable.DefaultChannel")) {
                  Choice c = flapTable.CFlapTableWindow.ixpNames;
                  c.addItemListener (il);
                  // find the current preference for this choice 
                  // and make it the first element.
                  String s1 = (String) prefs.get ("FlapTable",
                     "DefaultChannel");
                  String currentChoice = s1.substring (s1.indexOf (":") + 1);
                  prefLabelPanel.add (new Label ("DefaultChannel: "));
                  prefChoicePanel.add (c);
                  c.select (currentChoice);
                  c.repaint ();
               }
            }
            
            prefFrame.pack();
            prefFrame.setVisible (true);
            prefChoicePanel.repaint (100);
            return;
         }

         if (label.equals (AS)) {
            prefLabelPanel.add (new Label ("ASExplorer Options:"));
            prefChoicePanel.add (valueLabel);
            
            int vecsize = ASExplorerVec.size ();
            
            for (int i = 0; i < vecsize; i++) {
               String anItem = (String) ASExplorerVec.elementAt (i);
               if (anItem.equals ("ASExplorer.DefaultChannel")) {
                  Choice c = asExplorer.CASExplorerFrame.ixpNames;
                  c.addItemListener (il);
                  // find the current preference for this choice 
                  // and make it the first element.
                  String s1 = (String) prefs.get ("ASExplorer",
                     "DefaultChannel");
                  String currentChoice = s1.substring (s1.indexOf (":") + 1);
                  prefLabelPanel.add (new Label ("DefaultChannel: "));
                  prefChoicePanel.add (c);
                  c.select (currentChoice);
                  c.repaint ();
               }
            }
            
            prefFrame.pack();
            prefFrame.setVisible (true);
            prefChoicePanel.repaint (100);
            return;
         }

         if ((label.equals (OKB)) ||
             (label.equals (SAVEB))){

            if (label.equals (OKB)) {
               prefFrame.setVisible (false);

               // if nothings changed - dont save anything. just
               // delete the frame.
               
               if (!stateChange) {
                  prefFrame.removeAll();
                  prefFrame = null;
                  return;
               }
            }

            if ((label.equals (SAVEB)) &&
                (!stateChange)) {
               // nothing has changed, do nothing
               return;
            }


            // else save for each application, and common 
            // values into the preference table.

            if (CommonVec.size () > 0) {
            }

            if (FlapGraphVec.size () > 0) {
               // set default channel
               Choice c = flapGraph.CFlapGraphWindow.ixpNames;
               String newVal = c.getSelectedItem();
               prefs.set ("FlapGraph", "DefaultChannel", "FlapGraph:" +
                                                         newVal + ":all");
            }
            
            if (FlapTableVec.size () > 0) {
               // set default channel
               Choice c = flapTable.CFlapTableWindow.ixpNames;
               String newVal = c.getSelectedItem();
               prefs.set ("FlapTable", "DefaultChannel", 
                  "FlapTableDaily:" + newVal);
            }

            if (ASExplorerVec.size () > 0) {
               // set default channel
               Choice c = asExplorer.CASExplorerFrame.ixpNames;
               String newVal = c.getSelectedItem();
               prefs.set ("ASExplorer", "DefaultChannel", 
                  "ASExplorer:" + newVal);
            }

            // do some final cleanup
            if (label.equals (OKB)) {
               prefFrame.removeAll ();
               prefFrame = null;
            }

            prefs.save();
            stateChange = false;
            return;

         }

      }
      
   }


   class InnerItemAdapter implements ItemListener {
      public void itemStateChanged (ItemEvent e) {
         stateChange = true;
      }
   }


   class InnerMouseMotionAdapter extends MouseMotionAdapter {

      public void mouseMoved (MouseEvent e) {
         
         if ((e.getSource() instanceof Button)) {
            Button tmpButton = (Button) e.getSource();

            if (tmpButton == okB) {
               bottomText.setText ("New defaults are assumed at startup");
            }
            else if (tmpButton == cancelB) {
               bottomText.setText ("All actions after the last Save cancelled");
            }

            else if (tmpButton == showChoiceB) {
               bottomText.setText ("Shows currently saved configuration");
            }

            else if (tmpButton == saveB) {
               bottomText.setText ("Saves choices made so far.\nNew defaults are assumed at startup");
            }
            
         }

         if (e.getSource() instanceof Panel) {
            if ((Panel) e.getSource() == bottomButtonPanel) {
               bottomText.setText (" ");
               bottomPanel.repaint (100);
               return;
            }
         }
      }
   }
}
