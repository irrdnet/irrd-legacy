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




/**
 * CMenuCreator
 * 
 * CMenuCreator is a utility class for creating menus.
 * 
 * Modification History:
 * Aded routines to support lists rather than menus, and also
 * to add and remove listeners from menus and lists.
 * @version 1.1.
 * @date 9 June 1998.
 * @author Sushila Subramanian,
 *
 * @version 1.0
 * @date 
 * @author Jimmy Wan
 */



import java.awt.event.*;
import java.awt.*;

import debug.*;

public abstract class CMenuCreator {
  
  /** CreateMenu: Creates a menu given a name and items.
   *  @param MenuName Name for menu.
   *  @param MenuItemName Array of menu items.
   *  @return Menu constructed from input items.
   */
  
  public static final Menu  CreateMenu(String MenuName, 
				       String MenuItemName[]) {
    
    Menu  menu = new Menu(MenuName);
    int   NumMenuItems = MenuItemName.length;
    int   i = 0;
    do {
		if (!ipma.PlugIn.StartApps.isApplication){
		  if (MenuItemName[i].equals("Save") ||
				MenuItemName[i].equals("Save & Exit")){
		  }
		  else{
			 menu.add(new MenuItem(MenuItemName[i]));
		  }
		}
		else{
		  menu.add(new MenuItem(MenuItemName[i]));
		}
    } while (++i < NumMenuItems);
    return menu;
  }
  
  /** CreateMenuBar: Creates a menubar, with menus provided.
   *  @param MenuName Array of menus that need to go on menubar.
   *  @param MenuItems 2D Array of menu items corresponding to
   *  above menus.
   *  @return MenuBar constructed with input items.
   */

  public static final MenuBar  CreateMenuBar(String MenuName[], 
					     String MenuItemName[][]) {
    
    MenuBar  menuBar = new MenuBar();
    Menu     menu;
    MenuItem menuItem;
    
    int   i = 0,
      NumMenus = MenuName.length,
      j,
      NumMenuItems;
    do {
      j = 0;
      NumMenuItems = MenuItemName[i].length;
      menu = new Menu(MenuName[i]);
      do {
		  if (!ipma.PlugIn.StartApps.isApplication){
			 if (MenuItemName[i][j].equals("Save") ||
				  MenuItemName[i][j].equals("Save & Exit")){
			 }
			 else{
				menu.add(new MenuItem(MenuItemName[i][j]));
			 }
		  }
		  else{
			 menu.add(new MenuItem(MenuItemName[i][j]));
		  }
      } while (++j < NumMenuItems);
      menuBar.add(menu);
    } while (++i < NumMenus);
    return menuBar;
  }

  /** addRecursiveListener: Adds a given actionlistener 
   *  recursively to MenuItems in a menu. (i.e. only adds
   *  listeners to MenuItems, not to Menus that may be 
   *  submenus).
   *  @param Menu Menu to add listeners to.
   *  @param al Action listener to be added.
   */
  
  public static void addRecursiveListener (Menu menu, ActionListener al) {

     MenuItem menuItem;

     for (int j = 0; j < menu.getItemCount (); j++)  {
       menuItem = menu.getItem (j);
       if (menuItem instanceof java.awt.Menu) {
          //call this routine recursively
          CMenuCreator.addRecursiveListener ((Menu) menuItem, al);
       }
       else {
          menuItem.addActionListener (al);
       }
     }  
  }

  /** removeRecursiveListener: Recursively goes down a menu
   *  through all the submenus and removes listeners for 
   *  leaf menu items.
   *  @param menu Menu to recurse through.
   *  @param al Action listener to be removed.
   */

  public static void removeRecursiveListener (Menu menu, ActionListener al) {

     MenuItem menuItem;

     for (int j = 0; j < menu.getItemCount (); j++)  {
       menuItem = menu.getItem (j);
       if (menuItem instanceof java.awt.Menu) {
          //call this routine recursively
          CMenuCreator.removeRecursiveListener ((Menu) menuItem, al);
       }
       else
          menuItem.removeActionListener (al);
     }  
  }

  /* addRecursiveItemListener: Recursively add listeners to 
   * Checkbox items in a menu. (i.e. ignore submenus, and only
   * add listeners for CheckboxMenuItem).
   * @param menu Menu to recurse through.
   * @param il Item listener to be added.
   */

  public static void addRecursiveItemListener (Menu menu, ItemListener il) {

     MenuItem menuItem;

     for (int j = 0; j < menu.getItemCount (); j++)  {
       menuItem = menu.getItem (j);
       if (menuItem instanceof java.awt.Menu) {
          //call this routine recursively
          CMenuCreator.addRecursiveItemListener ((Menu) menuItem, il);
       }
       else {
         if (menuItem instanceof CheckboxMenuItem)
          ((CheckboxMenuItem) menuItem).addItemListener (il);
       }
     }  
  }

  /** removeRecursiveItemListener: Recurse through a menu and
   *  remove listeners from CheckboxMenuItems. 
   *  @param menu Menu to recurse through.
   *  @param il Item listener to remove.
   */

  public static void removeRecursiveItemListener (Menu menu, ItemListener il) {

     MenuItem menuItem;

     for (int j = 0; j < menu.getItemCount (); j++)  {
       menuItem = menu.getItem (j);
       if (menuItem instanceof java.awt.Menu) {
          //call this routine recursively
          CMenuCreator.removeRecursiveItemListener ((Menu) menuItem, il);
       }
       else
          ((CheckboxMenuItem) menuItem).removeItemListener (il);
     }  
  }

}



