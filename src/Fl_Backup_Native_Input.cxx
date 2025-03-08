//
//  Fl_Backup_Native_Input.cxx
//

#ifndef FL_DOXYGEN

#include "Fl_Native_Input_Driver.H"


Fl_Native_Input_Driver *Fl_Native_Input_Driver::newNativeInputDriver(Fl_Native_Input *n) {
  Fl_Native_Input_Driver *retval = new Fl_Backup_Native_Input_Driver();
  retval->widget = n;
  return retval;
}

#endif // ndef FL_DOXYGEN
