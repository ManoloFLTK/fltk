//
// Dockable window header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 2025 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

/* \file
   Fl_Dockable_Group widget . */

#ifndef Fl_Dockable_Group_H
#define Fl_Dockable_Group_H

#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>

class Fl_Dockable_Group_Driver;

/** A group that can be detached from its parent, dragged with the mouse to an adequate widget, and docked there.
 Like any Fl_Group, an Fl_Dockable_Group can contain any set of Fl_Widget's.
 An  Fl_Dockable_Group object becomes detachable and draggable to some destination widget
 after member function drag_box(int x, int y, int w, int h) is called once. This adds an Fl_Box
 to the Fl_Dockable_Group that can be called its  'drag box'. Clicking this 'drag box' and dragging the mouse
 detaches the Fl_Dockable_Group from its parent and allows to drag it away in the form of a functional Fl_Window.
 An Fl_Dockable_Group::Dockable_Box object replaces the Fl_Dockable_Group in its parent Fl_Group
 unless this group's processing of the FL_UNDOCK event produces another layout.

 A widget becomes susceptible to receive a dragged Fl_Dockable_Group object by making its \p handle()
 member function respond to events FL_DOCK_ENTER, FL_DOCK_DRAG, FL_DOCK_RELEASE, FL_DOCK_LEAVE
 and FL_UNDOCK. Subclass Fl_Dockable_Group::Dockable_Box is a basic widget able to dock Fl_Dockable_Group
 objects as follows: an Fl_Box object with label "Dock here" receives any Fl_Dockable_Group
 that would be dragged and dropped above. Any Fl_Dockable_Group docked at the
 Fl_Dockable_Group::Dockable_Box can be undocked and dragged away. The docking
 location appears again as an Fl_Box with label "Dock here".
 
 When a dragged Fl_Dockable_Group object is positioned above a receiving widget and the mouse
 is released, the Fl_Dockable_Group is docked there, that is, stops appearing as a movable window and,
 if the receiving widget is an Fl_Group, becomes a child of this group, otherwise a child of the widget's
 parent group. Usually it also acquires the position and size of the receiving widget.
 
 Example program \p test/dock_undock.cxx shows how to implement the handle member function
 of a class derived from Fl_Tabs and obtain this behavior: any Fl_Dockable_Group object that's a child
 of this class can be removed from the Fl_Tabs and dragged away; If an Fl_Dockable_Group object is dragged
 above the <u>tabs area</u> of the Fl_Tabs-derived object and the mouse is released, the
 Fl_Dockable_Group becomes a new tab.
 
 Member functions color_for_states() and label_for_states() control the colors of a  Fl_Dockable_Group's
 drag box and its label during each of the 4 enum Fl_Dockable_Group::states it can be in. These functions' default argument
 values indicate the colors and labels FLTK uses by default. Calling these member functions allows to
 customize such colors and labels.
  
 \note Platform details:
 <ul><li>Under Wayland, Fl_Dockable_Group dragging and docking is performed via a Drag-and-Drop operation which itself
 can be implemented in 2 ways.
 <ul><li>Recent versions of the Mutter (Gnome ≥ 48) and KWin (Qt ≥ 6.3) compositors support a Wayland
 protocol called <tt>XDG toplevel drag</tt> that allows to detach the Fl_Dockable_Group, transform it into a
 draggable window, and drop it to a receiving widget back into the form of an Fl_Dockable_Group.
 The KWin compositor's support of <tt>XDG toplevel drag</tt> forbids an Fl_Dockable_Group to be part of a subwindow.
 <li>When the active Wayland compositor doesn't support that protocol, FLTK computes a small image of the Fl_Dockable_Group and
 uses it as the cursor image of a DnD operation that ends being dropped to a 'target box' or being cancelled.
 </ul>
 <li>Under other platforms, Fl_Dockable_Group dragging and docking begins by transforming the group into an Fl_Window which is
 dragged around with the mouse and can be docked to any receiving location.
 </ul>
*/
class Fl_Dockable_Group : public Fl_Group {
#ifndef FL_DOXYGEN
  friend class Fl_Dockable_Group_Driver;
#endif
private:
  Fl_Dockable_Group_Driver *driver_;
  Fl_Box *drag_box_;
  void color_targets_following_dock_();
public:
  Fl_Dockable_Group(int x, int y, int w, int h, const char *t=0);
  /** Basic box widget able to dock an Fl_Dockable_Group dragged above it.
   An Fl_Dockable_Group dragged and dropped above this widget replaces it acquiring
   its position and size.
   An Fl_Dockable_Group docked above this widget can be dragged away showing the
   basic box widget again.
   Use member function Fl_Dockable_Group::newDockableBox to create an object
   of this class.
   The box label is "Dock here" by default and can be changed.
   */
  class Dockable_Box : public Fl_Box {
    friend class Fl_Dockable_Group_Driver;
  protected:
    Dockable_Box(int x, int y, int w, int h);
  public:
    static const char *dockable_label;
#ifndef FL_DOXYGEN
    int handle(int event) override;
    void state(bool can_dock);
#endif // ndef FL_DOXYGEN
  };
  Fl_Box *drag_box(int x, int y, int w, int h);
  /** Returns the Fl_Dockable_Group's 'drag box' */
  Fl_Box *drag_box() { return drag_box_; }
  static Fl_Dockable_Group::Dockable_Box *newDockableBox(int x, int y, int w, int h);
  void after_release();
  void color_for_states(Fl_Color undock = FL_BACKGROUND_COLOR, Fl_Color drag = FL_BACKGROUND2_COLOR,
                        Fl_Color dock = FL_SELECTION_COLOR, Fl_Color dragged = FL_SELECTION_COLOR);
  void label_for_states(const char *undock = "Undock", const char *drag = "Drag",
                        const char *dock = "Dock", const char *dragged = "Dragged");
  /**
   The 4 states that an Fl_Dockable_Group and its 'drag box' can adopt during their lifetime.
   */
  enum states {
    UNDOCK, ///< The Fl_Dockable_Group is ready to be dragged away by a click-drag on its drag box.
    DRAG, ///<  The Fl_Dockable_Group is being dragged away.
    DOCK, ///< The Fl_Dockable_Group is above a receiving widget where it will dock if the mouse is released.
    DRAGGED ///<  Used only when the Fl_Dockable_Group is represented by an icon and is being dragged.
  };
private:
    enum states state_; // The current state of the Fl_Dockable_Group
public:
  void state(enum states s);
  /** Gets the state of the Fl_Dockable_Group */
  enum states state() {return state_; }
  /** Non-NULL when an Fl_Dockable_Group is currently active.
   Under Wayland, this means a Drag-and-Drop of a window (or an image of a window) is active.
   Under other platforms, this means an Fl_Dockable_Group is currently above a target box
   and can be docked there.
   */
  static Fl_Dockable_Group *active_dockable;
  static bool is_dockable_inside(Fl_Widget *);
};

#endif // Fl_Dockable_Group_H
