README.Wayland.txt - Wayland platform support for FLTK
------------------------------------------------------


CONTENTS
========

 1   INTRODUCTION

 2   WAYLAND SUPPORT FOR FLTK
   2.1    Configuration
   2.2    Currently unsupported features

 3   PLATFORM SPECIFIC NOTES
   3.1    Debian and Derivatives (like Ubuntu)

 4   DOCUMENT HISTORY


1 INTRODUCTION
==============

The support of the Wayland platform is work in progress of the FLTK library.
This work is quite advanced, though : all test/ and examples/ programs build and run,
and CJK text-input methods as well as dead and compose keys are supported.
It requires a Wayland-equipped OS which means Linux.
The code has been tested on Debian and Ubuntu with 3 distinct compositors: mutter, weston, and KDE.


2 WAYLAND SUPPORT FOR FLTK
==========================

It is possible to have your FLTK application do all its windowing and drawing
through the Wayland protocol on Linux systems. All drawing is done via Cairo or EGL.
All text-drawing is done via Pango.

 Configuration
---------------

* Configure-based build can be performed as follows:
Once after "git clone/git checkout wayland", create the configure file :
   autoconf -f

Prepare build with :
   ./configure --enable-wayland [--enable-shared]
   
Build with :
   make

* CMake-based build can be performed as follows:
cmake -S <path-to-source> -B <path-to-build> -DCMAKE_BUILD_TYPE=Release -DOPTION_USE_WAYLAND=1

cd <path-to-build>; make

The FLTK wayland platform uses a library called libdecor which handles window decorations
(i.e., titlebars, shade). Libdecor is bundled in the FLTK source code and FLTK uses by default
this form of libdecor. Optionally, OPTION_USE_SYSTEM_LIBDECOR can be turned on to have FLTK
use the system's version of libdecor which is available on recent Linux distributions (e.g.,
Debian bookworm or more recent in packages libdecor-0-0 and libdecor-0-plugin-1-cairo).

 Currently unsupported features
-------------------------------

* With Wayland, there is no way to know if a window is currently minimized, nor is there any
way to unset minimization on this window. Consequently, Fl_Window::show() of a minimized
window does nothing.
* A deliberate design trait of Wayland makes application windows ignorant of their exact
placement on screen. It's possible, though, to position a popup window relatively to another
window. This allows FLTK to properly position menu and tooltip windows. But Fl_Window::position()
has no effect on other top-level windows.
* FLTK currently doesn't prevent menu windows from expanding beyond screen borders.


3 PLATFORM SPECIFIC NOTES
=========================

The following are notes about building FLTK for the Wayland platform
on the various supported Linux distributions.

    3.1 Debian and Derivatives (like Ubuntu)
    ----------------------------------------
These packages are necessary, in addition to those for usual X11-based platforms :
- libwayland-dev
- wayland-protocols
- libdbus-1-dev
- libxkbcommon-dev
- libegl-dev
- libopengl-dev
- libpango1.0-dev
- libcairo2-dev
- libgtk-3-dev <== with this, windows get a GTK-style titlebar

4 DOCUMENT HISTORY
==================

May 29 2021 - Manolo: Initial version.
Oct 28 2021 - Manolo: --enable-shared configure option is now supported.
Nov 21 2021 - Manolo: CMake-based building is now supported.
Dec 16 2021 - Manolo: present CMake option OPTION_USE_SYSTEM_LIBDECOR
Dec 28 2021 - Manolo: added support for text input methods.
