package ipma.Window;

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
 * ColoredMultiList
 * 
 * Sets up columns, each of which is a list and whose items are set
 * to prespecified colours.
 * 
 * Modification History:
 * Original code is in java 1.0.2. This version is a port to jdk 1.1
 *
 * $Log:
 */

import java.awt.*;
import java.util.Vector;

public class ColoredMultiList
extends Canvas {
  
  public static final int INITIAL_ENTRIES   = 8;
  public static final int INITIAL_INCREMENT = 8;
  
  static final Color   DEFAULT_COLOR = Color.black;
  
  protected FontMetrics   fm;
  
  protected Vector  Items[];
  public Vector fItems;
  protected Vector  Colors[];
  
  protected int     Offset[];
  
  protected int     StartHeight;
  public int     StringHeight;
  protected int     StringWidth;
  protected int     CharacterWidth;
  
  protected int     SelectedItem;
  
  protected int     NumColumns;
  protected int     ItemLength;
  
  private boolean   NewFont;

  /** ColoredMultiList: Constructor.
   */
  
  private ColoredMultiList() {
    super();
  }

  /** ColoredMultiList: Constructor with num columns.
   * @param NumColumns Number of columns in list.
   */
  
  public ColoredMultiList(int NumColumns) {
    super();
    if (NumColumns <= 0)
      NumColumns = 1;
    
    this.NumColumns = NumColumns;
    Items    = new Vector[NumColumns];
    // list of items that have been selected for display. 
    // f for filter.
    fItems = new Vector ();
    Colors   = new Vector[NumColumns];
    Offset   = new int[NumColumns];
    
    int   i = 0;
    
    fItems = new Vector (INITIAL_ENTRIES, INITIAL_INCREMENT);

    do {
      Items[i]    = new Vector(INITIAL_ENTRIES, INITIAL_INCREMENT);
      Colors[i]   = new Vector(INITIAL_ENTRIES, INITIAL_INCREMENT);
      Offset[i]   = i << 4;
    } while (++i < NumColumns);
    
    NewFont = true;

    Font  font;
    
    font = new Font("Monospaced", Font.PLAIN, 10);

    if (font != null)
      setFont(font);
    
    FontMetrics Newfm = getFontMetrics(font);
    StringHeight = Newfm.getLeading() + Newfm.getMaxAscent() +
      Newfm.getMaxDescent();
    StringWidth = Newfm.getMaxAdvance();
    
  }
    
  /** AddItem: Adds an array of items.
   *  @param item Array of items.
   */

  public void AddItem(String item[]) {
    AddItem(item, DEFAULT_COLOR);
  }
  
  /** AddItem: Adds an array of items at a specified index.
   * @param item Array of items.
   * @param index Index to insert at.
   */
  
  public void AddItem(String item[], int index) {
    AddItem(item, index, DEFAULT_COLOR);
  }

  /** AddItem: Adds array of items of a specified colour.
   * @param item Array of items.
   *  @param c Color of items.
   */  
  
  synchronized public void AddItem(String item[], Color c) {
    int   i = 0;
    do {
      Items[i].addElement(item[i]);
      Colors[i].addElement(c);
    } while (++i < NumColumns);
  }
  
  /** AddItem: Adds an array of items at an index with a 
   *  specified colour.
   *  @param item Items to be added.
   *  @param index Index to insert at.
   *  @param c Colour for new items.
   */
  
  synchronized public void AddItem(String item[], int index, Color c) {
    int   i = 0;
    do {
      Items[i].insertElementAt(item[i], index);
      Colors[i].insertElementAt(c, index);
    } while (++i < NumColumns);
  }
  
  /** RemoveAllItems: Removes all items from the list.
   */

  synchronized public void RemoveAllItems() {
    int   i = 0;
    
    do {
      Items[i].removeAllElements();
      Colors[i].removeAllElements();
    } while (++i < NumColumns);
    Deselect();
  }
  
  
  public void Deselect() {
    SelectedItem = -1;
  }
  
  
  public void Select(int index) {
    SelectedItem = index;
  }
  
  /** SetOffset: Sets the offset for a given column.
   * @param Column Column to be offset from left edge.
   * @param Value  Offset value.
   */
  
  synchronized public final void SetOffset(int Column, int Value) {    
    Offset[Column] = Value;
  }
  
  
  synchronized public final void setFont(Font f) {
    super.setFont(f);
    NewFont = true;
  }
  
  
  synchronized public void paint(Graphics g) {
    boolean filtered = false;
    if (NewFont) {
      FontMetrics Newfm = g.getFontMetrics();
      fm = Newfm;
      StringHeight = Newfm.getLeading() + Newfm.getMaxAscent() +
   Newfm.getMaxDescent();
      
      StartHeight = Newfm.getMaxAscent();
      CharacterWidth = Newfm.getMaxAdvance();
      if (CharacterWidth == -1)
   CharacterWidth = Newfm.stringWidth("0");
    }
    
    int      y = StartHeight;
    int      i = 0,
      j= 0, NumItems, pointer = 0;
      //NumItems = Items[0].size();
    
    String   CurrentItem;
    Color    CurrentColor;

    /* 
    if (fItems.size() == 0 ){
      NumItems = Items[0].size();
      CurrentItem    = (String)Items[0].elementAt(0); //just for initialization
    }
    else { //filtering is set
      filtered = true;
      NumItems = fItems.size();
      pointer = Integer.parseInt(fItems.elementAt(0).toString());
      CurrentItem    = (String)Items[0].elementAt(pointer); 
    }
    */

    /* do special case for instanceof CParticipantsCanvas coz - things
     * wont be filtered there. need to be careful here - if any other
     * application uses ColoredMultiList other than IPN - need to make
     * this first section more generic. Nancy appears to have assumed
     * nothing other than the main panel will use this object - and 
     * so made the "filtered item case" the default. -sushi
     */
    
/*
    if (this instanceof CParticipantsCanvas) {
       NumItems = Items[0].size();
       while (i < NumItems) {
          j = 0;
          do {
             CurrentItem    = (String)Items[j].elementAt(i);
             if ((j+1)< NumColumns && CurrentItem.length()>(Offset[j+1]-Offset[j]))
                CurrentItem=CurrentItem.substring(0,Offset[j+1]-Offset[j]-1);
             CurrentColor   = (Color)Colors[j].elementAt(i);
             g.setColor(CurrentColor);
             g.drawString(CurrentItem,
                Offset[j] * CharacterWidth, y);
          }while (++j < NumColumns);
          y += StringHeight;
          i++;
       }
    }
*/

    if (fItems.size() !=0){

      filtered = true;
      NumItems = fItems.size();
      if (NumItems == 0) return;
      pointer = Integer.parseInt(fItems.elementAt(0).toString());
      CurrentItem    = (String)Items[0].elementAt(pointer); 
      CurrentColor   = (Color)Colors[0].elementAt(pointer);
    
      
      while (i < NumItems) {
   j = 0;
   do {
     if (filtered) {
       pointer = Integer.parseInt(fItems.elementAt(i).toString());
       CurrentItem    = (String)Items[j].elementAt(pointer);
       CurrentColor   = (Color)Colors[j].elementAt(pointer);
     }
     /*
       else{
       CurrentItem    = (String)Items[j].elementAt(i);
       CurrentColor   = (Color)Colors[j].elementAt(i);
       }   
     */
   //   if (!Items[0].elementAt(i).equals("D")&&!Items[0].elementAt(i).equals("R") )
     if ((j+1)< NumColumns && CurrentItem.length()>(Offset[j+1]-Offset[j]))
       CurrentItem=CurrentItem.substring(0,Offset[j+1]-Offset[j]-1);
     //  CurrentColor   = (Color)Colors[j].elementAt(i);
     g.setColor(CurrentColor);
     g.drawString(CurrentItem,
           Offset[j] * CharacterWidth, y);
   }while (++j < NumColumns);
   //  if (!Items[0].elementAt(i).equals("D")&&!Items[0].elementAt(i).equals("R"))
   y += StringHeight;
   i++;
      }
    }
  }
  
  
  public void update(Graphics g, int offsetX, int offsetY){
    g.clearRect(0,0,getSize().width, getSize().height);
    g.translate(offsetX, offsetY);
    paint(g);
  }
  
  public final Dimension CanvasSize() {
    //   return new Dimension(StringWidth *OutageListLength, StringHeight * OutageList.size());
    return new Dimension(StringWidth *ItemLength, StringHeight * Items[0].size());
  }
  
  public String[] getTitleString(){return new String[1];}
  
  public int[]  getOffset(){
    return(Offset);
  }
  
  public int getNumColumns(){
    return(NumColumns);
  }
  
  public final int StringHeight() {
    return StringHeight;
  }
  
  public final int StringWidth() {
    return StringWidth;
  }
}
