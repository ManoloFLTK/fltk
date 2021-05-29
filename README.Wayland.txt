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
This work is quite advanced, though : all test/ and examples/ programs build and run.
It requires a Wayland-equipped OS which means Linux.
The code has been tested on Debian and Ubuntu.


2 WAYLAND SUPPORT FOR FLTK
==========================

It is possible to have your FLTK application do all its windowing and drawing
through the Wayland protocol on Linux systems. All drawing is done via Cairo or EGL.

 Configuration
---------------

At this point, only configure-based build is available.
Once after "git clone", create the configure file :
   autoconf -f

Prepare build with :
   ./configure --enable-wayland
   
Build with :
   make


 Currently unsupported features
-------------------------------

* "make install" is not supported. All apps use a dynamic library, libdecor-0.1.so, currently put
in the lib/ subdirectory of the FLTK source tree.
* drag-and-drop is not implemented  (but copy/paste of text and images is supported)
* complex text-input methods are not supported (but dead and compose keys are supported)



3 PLATFORM SPECIFIC NOTES
=========================

The following are notes about building FLTK for the Wayland platform
on the various supported Linux distributions.

    3.1 Debian and Derivatives (like Ubuntu)
    ----------------------------------------
These packages are necessary, beyond those for usual X11-based platforms :
- libwayland-dev
- wayland-protocols
- libdbus-1-dev
- libxkbcommon-dev
- libegl-dev


4 DOCUMENT HISTORY
==================

May 29 2021 - Manolo: Initial version.
