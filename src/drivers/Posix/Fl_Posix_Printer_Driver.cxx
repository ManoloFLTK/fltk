//
// PostScript priting support for the Fast Light Tool Kit (FLTK).
//
// Copyright 2010-2025 by Bill Spitzak and others.
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

#include <config.h>

#if !defined(FL_NO_PRINT_SUPPORT)

#include <FL/Fl_PostScript.H>
#include "../PostScript/Fl_PostScript_Graphics_Driver.H"
#include <FL/Fl_Printer.H>
#include <FL/fl_ask.H>
#include <FL/Fl_PDF_File_Surface.H>

#include <src/print_panel.cxx>

/** Support for printing on the Unix/Linux platform */
class Fl_Posix_Printer_Driver : public Fl_PostScript_File_Device {
  int begin_job(int pagecount = 0, int *frompage = NULL, int *topage = NULL, char **perr_message=NULL) FL_OVERRIDE;
};

#if HAVE_DLSYM && HAVE_DLFCN_H
// GTK types
#include <dlfcn.h>   // for dlopen et al
#include <unistd.h>  // for mkstemp
#include <FL/filename.H>
#include "Fl_Posix_System_Driver.H"
#define GTK_PAPER_NAME_LETTER "na_letter"
#define GTK_PAPER_NAME_LEGAL "na_legal"
#define GTK_PAPER_NAME_A3 "iso_a3"
#define GTK_PAPER_NAME_A5 "iso_a5"
#define GTK_PAPER_NAME_JB5 "jis_b5"
#define GTK_PAPER_NAME_TABLOID "na_ledger"
#define GTK_PAPER_NAME_DLE "iso_dl"
#define GTK_RESPONSE_NONE -1
#define GTK_RESPONSE_REJECT -2
#define GTK_RESPONSE_OK -5
#define GTK_RESPONSE_CANCEL -6
#define GTK_RESPONSE_DELETE_EVENT -4
#define GTK_PRINT_PAGES_RANGES 2

/* Class Fl_GTK_Printer_Driver uses libgtk3 or libgtk4 to construct a printer chooser dialog.
   That dialog is from class GtkPrintUnixDialog unless GTK version ≥ 4.14 runs which allows to use
   class GtkPrintDialog */
class Fl_GTK_Printer_Driver : public Fl_PDF_File_Surface {
public:
  typedef int gboolean;
  struct GtkPrintUnixDialog;
  struct GtkDialog;
  struct GtkPrintSettings;
  struct GtkPageSetup;
  enum GtkPageOrientation {GTK_PAGE_ORIENTATION_PORTRAIT, GTK_PAGE_ORIENTATION_LANDSCAPE};
  struct GtkPaperSize;
  struct GtkPrinter;
  struct GtkPrintJob;
  struct GtkWidget;
  struct GError;
  struct GtkWindow;
  struct GMainContext;
  struct GtkPrintDialog;
  struct GObject;
  struct GCancellable;
  struct GAsyncResult;
  struct GtkPrintSetup;
  struct GFile;

  GtkPrintJob *pjob; // data shared between begin_job() and end_job()
  GtkPrintDialog *dialog414;
  GtkPrintSetup *print_setup;
  GFile *gfile; // when using GtkPrintDialog and not printing to file
  char tmpfilename[50]; // name of temporary PostScript file containing to-be-printed data
  GMainContext *main_context;
  int begin_job(int pagecount, int *frompage, int *topage, char **perr_message) FL_OVERRIDE;
  void end_job() FL_OVERRIDE;
  static bool probe_for_GTK();
  static void *ptr_gtk; // points to the GTK dynamic lib or NULL

  typedef GtkPrintUnixDialog* (*gtk_print_unix_dialog_new_t)(const char*, void*);
  typedef int (*gtk_dialog_run_t)(GtkDialog*);
  typedef GtkPrintSettings *(*gtk_print_unix_dialog_get_settings_t)(GtkPrintUnixDialog*);
  typedef void (*gtk_print_unix_dialog_set_settings_t)(GtkPrintUnixDialog*, GtkPrintSettings*);
  typedef GtkPageSetup *(*gtk_print_unix_dialog_get_page_setup_t)(GtkPrintUnixDialog*);
  typedef GtkPageOrientation (*gtk_page_setup_get_orientation_t)(GtkPageSetup*);
  typedef GtkPaperSize* (*gtk_page_setup_get_paper_size_t)(GtkPageSetup*);
  typedef const char * (*gtk_paper_size_get_name_t)(GtkPaperSize*);
  typedef GtkPrinter * (*gtk_print_unix_dialog_get_selected_printer_t)(GtkPrintUnixDialog*);
  typedef int (*gtk_printer_accepts_pdf_t)(GtkPrinter*);
  typedef int (*gtk_printer_is_active_t)(GtkPrinter*);
  typedef GtkPrintJob *(*gtk_print_job_new_t)(const char *, GtkPrinter *, GtkPrintSettings *, GtkPageSetup *);
  typedef void (*gtk_widget_hide_t)(GtkWidget*);
  typedef void (*gtk_widget_destroy_t)(GtkWidget*);
  typedef void (*gtk_window_destroy_t)(GtkWindow*);
  typedef gboolean (*gtk_events_pending_t)(void);
  typedef gboolean (*g_main_context_pending_t)(void*);
  typedef void (*g_main_context_iteration_t)(GMainContext*, bool);
  typedef GMainContext* (*g_main_context_default_t)(void);
  typedef void (*gtk_main_iteration_t)(void);
  typedef int (*gtk_print_job_set_source_file_t)(GtkPrintJob *job, const char *filename, GError **error);
  typedef void (*gtk_print_job_send_t)(GtkPrintJob *, void* , gboolean* , void* );
  typedef void (*gtk_print_settings_set_t) (GtkPrintSettings *settings, const char *key, const char *value);
  typedef const char * (*gtk_print_settings_get_t) (GtkPrintSettings *settings, const char *key );
  typedef int (*gtk_print_settings_get_print_pages_t)(GtkPrintSettings*);
  struct GtkPageRange { int start, end; };
  typedef GtkPageRange* (*gtk_print_settings_get_page_ranges_t)(GtkPrintSettings*, int*);
  typedef void (*g_object_unref_t)(void* object);
  typedef struct _GClosure GClosure;
  typedef void  (*GClosureNotify)(void* data, GClosure *closure);
  typedef void  (*GCallback)(void);
  typedef void (*g_signal_connect_data_t)(void *,const char *, GCallback, void*, GClosureNotify, int);
  typedef void (*gtk_print_unix_dialog_set_embed_page_setup_t)(GtkPrintUnixDialog *dialog, gboolean embed);
  typedef void (*gtk_widget_show_now_t)(GtkPrintUnixDialog *dialog);
  typedef void (*gtk_window_present_t)(GtkWindow *);
  typedef GtkWindow* (*gtk_window_new_t)();
  typedef GtkPrintDialog *(*gtk_print_dialog_new_t)();
  typedef void (*GAsyncReadyCallback)(GObject*, GAsyncResult*, void*);
  typedef void (*gtk_print_dialog_setup_t)(GtkPrintDialog*, GtkWindow*, GCancellable*,
                                           GAsyncReadyCallback, void *);
  typedef GtkPrintSetup* (*gtk_print_dialog_setup_finish_t)(GtkPrintDialog*, GAsyncResult*,GError**);
  typedef GFile* (*g_file_new_for_path_t)(const char*);
  typedef void (*gtk_print_dialog_print_file_t)(GtkPrintDialog*, GtkWindow*, GtkPrintSetup*,
                                            GFile*, GCancellable*, GAsyncReadyCallback, void *);
  typedef GtkPrintSettings* (*gtk_print_dialog_get_print_settings_t)(GtkPrintDialog*);
  typedef void (*gtk_print_dialog_set_print_settings_t)(GtkPrintDialog*, GtkPrintSettings*);
  typedef bool (*gtk_widget_is_visible_t)(GtkWidget*);
  typedef GtkPrintSettings* (*gtk_print_settings_new_t)();
  typedef GtkPageSetup *(*gtk_print_dialog_get_page_setup_t)(GtkPrintDialog*);
  typedef GtkPageSetup* (*gtk_print_setup_get_page_setup_t)(GtkPrintSetup*);
  typedef const char* (*gtk_print_settings_get_printer_t)(GtkPrintSettings*);
  typedef gboolean (*gtk_print_dialog_print_file_finish_t)(GtkPrintDialog*,GAsyncResult*,GError**);
  typedef GtkPrintSettings* (*gtk_print_setup_get_print_settings_t)(GtkPrintSetup*);
  typedef void (*gtk_print_setup_unref_t)(GtkPrintSetup*);
  struct response_and_printsetup {
    int *response;
    Fl_GTK_Printer_Driver::GtkPrintSetup *print_setup;
  };
  
  Fl_GTK_Printer_Driver() : Fl_PDF_File_Surface(), print_setup(NULL), gfile(NULL), main_context(NULL) {}
  static void print_dialog_response_cb(GtkPrintDialog*, GAsyncResult*, response_and_printsetup *);
  static void print_file_cb(GtkPrintDialog *, GAsyncResult*, int *);
};

// the CALL_GTK macro produces the source code to call a GTK function given its name
// or to get a pointer to this function :
// CALL_GTK(gtk_my_function) produces ((gtk_my_function_t)dlsym(ptr_gtk, "gtk_my_function"))
#define CALL_GTK(NAME) ((NAME##_t)dlsym(ptr_gtk, #NAME))

void *Fl_GTK_Printer_Driver::ptr_gtk = NULL;

// test wether GTK is available at run-time
bool Fl_GTK_Printer_Driver::probe_for_GTK() {
  Fl_Posix_System_Driver::probe_for_GTK(3, 0, &ptr_gtk);
  return (Fl_Posix_System_Driver::gtk_major_version > 0);
}

static void run_response_handler(void *dialog, int response_id, void* data)
{
  int *ri = (int *)data;
  *ri = response_id;
}

static int no_dispatch(int /*event*/, Fl_Window* /*win*/) {
  return 0;
}


void Fl_GTK_Printer_Driver::print_dialog_response_cb(GtkPrintDialog* dialog, GAsyncResult* res,
                                                     response_and_printsetup *pair) {
  GtkPrintSetup *setup = CALL_GTK(gtk_print_dialog_setup_finish)(dialog, res, NULL); // 4.14
  if (setup) {
    *(pair->response) = GTK_RESPONSE_OK;
    pair->print_setup = setup;
  } else {
    *(pair->response) = GTK_RESPONSE_CANCEL;
  }
}


int Fl_GTK_Printer_Driver::begin_job(int pagecount, int *firstpage, int *lastpage, char **perr_message) {
  enum Fl_Paged_Device::Page_Format format = Fl_Paged_Device::A4;
  enum Fl_Paged_Device::Page_Layout layout = Fl_Paged_Device::PORTRAIT ;

  GtkWindow *tmp_win = NULL;
  typedef int (*int_void_f_t)();
  int minor = ((int_void_f_t)dlsym(ptr_gtk, "gtk_get_minor_version"))();
  bool use_GtkPrintDialog = (100 * Fl_Posix_System_Driver::gtk_major_version + minor >= 414);
  if (Fl_Posix_System_Driver::gtk_major_version == 4 && !use_GtkPrintDialog) {
    // Create a temporary GtkWindow and use it as parent of the GtkPrintUnixDialog
    // which avoids message "GtkDialog mapped without a transient parent".
    tmp_win = CALL_GTK(gtk_window_new)();
  }
  
  GtkPrintUnixDialog *pdialog = NULL;
  dialog414 = NULL;
  GtkPrintSettings *psettings = NULL;
  int response_id = GTK_RESPONSE_NONE;
  struct response_and_printsetup data_pair = {&response_id, NULL};
  if (!use_GtkPrintDialog) {
    pdialog = CALL_GTK(gtk_print_unix_dialog_new)(Fl_Printer::dialog_title, tmp_win); //2.10
    CALL_GTK(gtk_print_unix_dialog_set_embed_page_setup)(pdialog, true); //2.18
    psettings = CALL_GTK(gtk_print_unix_dialog_get_settings)(pdialog); //2.10
  } else {
    dialog414 = CALL_GTK(gtk_print_dialog_new)(); // 4.14
    psettings = CALL_GTK(gtk_print_settings_new)();
  }
  char line[FL_PATH_MAX + 20];
  CALL_GTK(gtk_print_settings_set)(psettings, "output-file-format", "pdf"); //2.10
  char cwd[FL_PATH_MAX];
  snprintf(line, FL_PATH_MAX + 20, "file://%s/FLTK.pdf", fl_getcwd(cwd, FL_PATH_MAX));
  CALL_GTK(gtk_print_settings_set)(psettings, "output-uri", line); //2.10
  if (!use_GtkPrintDialog) {
    CALL_GTK(g_signal_connect_data)(pdialog, "response", GCallback(run_response_handler),
                                    &response_id, NULL,  0);
  } else {
    CALL_GTK(gtk_print_dialog_set_print_settings)(dialog414, psettings); // 4.14
    CALL_GTK(gtk_print_dialog_setup)(dialog414, NULL, NULL,  // 4.14
                                     (GAsyncReadyCallback)print_dialog_response_cb, &data_pair);
  }
  gtk_events_pending_t fl_gtk_events_pending;
  gtk_main_iteration_t fl_gtk_main_iteration;
  g_main_context_pending_t fl_g_main_context_pending;
  g_main_context_iteration_t fl_g_main_context_iteration;
  if (Fl_Posix_System_Driver::gtk_major_version < 4) {
    fl_gtk_events_pending = CALL_GTK(gtk_events_pending);
    fl_gtk_main_iteration = CALL_GTK(gtk_main_iteration);
    CALL_GTK(gtk_widget_show_now)(pdialog);
  } else {
    fl_g_main_context_pending = CALL_GTK(g_main_context_pending);
    fl_g_main_context_iteration = CALL_GTK(g_main_context_iteration);
    main_context = CALL_GTK(g_main_context_default)();
    if (pdialog) CALL_GTK(gtk_window_present)((GtkWindow*)pdialog);
  }
  Fl_Event_Dispatch old_dispatch = Fl::event_dispatch();
  // prevent FLTK from processing any event
  Fl::event_dispatch(no_dispatch);
  while (response_id == GTK_RESPONSE_NONE) { // loop that shows the GTK dialog window
    if (Fl_Posix_System_Driver::gtk_major_version < 4) fl_gtk_main_iteration(); // one iteration of the GTK event loop
    else {
      fl_g_main_context_iteration(main_context, false);
    }
    while (Fl::ready()) Fl::check(); // queued iterations of the FLTK event loop
  }
  if (tmp_win) CALL_GTK(gtk_window_destroy)(tmp_win);
  if (response_id == GTK_RESPONSE_OK) {
    GtkPageSetup *page_setup = NULL;
    if (!use_GtkPrintDialog) {
      page_setup = CALL_GTK(gtk_print_unix_dialog_get_page_setup)(pdialog); //2.10
    } else {
      print_setup = data_pair.print_setup;
      page_setup = CALL_GTK(gtk_print_setup_get_page_setup)(print_setup);
      psettings = CALL_GTK(gtk_print_setup_get_print_settings)(print_setup);
    }
    GtkPageOrientation orient = CALL_GTK(gtk_page_setup_get_orientation)(page_setup); //2.10
    if (orient == GTK_PAGE_ORIENTATION_LANDSCAPE) layout = Fl_Paged_Device::LANDSCAPE;
    GtkPaperSize* psize = CALL_GTK(gtk_page_setup_get_paper_size)(page_setup); //2.10
    const char *pname = CALL_GTK(gtk_paper_size_get_name)(psize); //2.10
    if (strcmp(pname, GTK_PAPER_NAME_LETTER) == 0) format = Fl_Paged_Device::LETTER;
    else if (strcmp(pname, GTK_PAPER_NAME_LEGAL) == 0) format = Fl_Paged_Device::LEGAL;
    else if (strcmp(pname, GTK_PAPER_NAME_A3) == 0) format = Fl_Paged_Device::A3;
    else if (strcmp(pname, GTK_PAPER_NAME_A5) == 0) format = Fl_Paged_Device::A5;
    else if (strcmp(pname, GTK_PAPER_NAME_JB5) == 0) format = Fl_Paged_Device::B5;
    else if (strcmp(pname, GTK_PAPER_NAME_TABLOID) == 0) format = Fl_Paged_Device::TABLOID;
    else if (strcmp(pname, GTK_PAPER_NAME_DLE) == 0) format = Fl_Paged_Device::DLE;
    GtkPrinter *gprinter = NULL;
    if (!use_GtkPrintDialog) {
      gprinter = CALL_GTK(gtk_print_unix_dialog_get_selected_printer)(pdialog); //2.10
      psettings = CALL_GTK(gtk_print_unix_dialog_get_settings)(pdialog); //2.10
    }
    const char* p = CALL_GTK(gtk_print_settings_get)(psettings, "output-uri"); //2.10;
    bool printing_to_file = (p != NULL);
    if (printing_to_file) {
      p += 6; // skip "file://" prefix
      strcpy(line, p);
      int l = strlen(p);
      if (strcmp(p+l-5, "/.pdf") == 0) {
        line[l-4] = 0;
        strcat(line, "FLTK.pdf");
      }
    }
    if (firstpage && lastpage) {
      *firstpage = 1; *lastpage = pagecount;
      if (CALL_GTK(gtk_print_settings_get_print_pages)(psettings) == GTK_PRINT_PAGES_RANGES) { // 2.10
        int num_ranges;
        GtkPageRange *ranges = CALL_GTK(gtk_print_settings_get_page_ranges)(psettings, &num_ranges); //2.10
        if (num_ranges > 0) {
          *firstpage = ranges[0].start + 1;
          *lastpage = ranges[0].end + 1;
          free(ranges);
        }
      }
    }
    response_id = GTK_RESPONSE_NONE;
    if (printing_to_file) {
      pjob = NULL;
      int err = Fl_PDF_File_Surface::begin_document(line, format, layout, perr_message);
      response_id = (err ? GTK_RESPONSE_REJECT : GTK_RESPONSE_OK);
      if (dialog414) CALL_GTK(g_object_unref)(dialog414);
    } else if ( !gprinter || (CALL_GTK(gtk_printer_accepts_pdf)(gprinter) && //2.10
        CALL_GTK(gtk_printer_is_active)(gprinter)) ) { // 2.10
      strcpy(tmpfilename, "/tmp/FLTKprintjobXXXXXX");
      int fd = mkstemp(tmpfilename);
      if (fd >= 0) {
        close(fd);
        Fl_PDF_File_Surface::begin_document(tmpfilename, format, layout);
        if (!use_GtkPrintDialog) {
          pjob = CALL_GTK(gtk_print_job_new)("FLTK print job", gprinter, psettings, page_setup); //2.10
        } else {
          pjob = NULL;
          gfile = CALL_GTK(g_file_new_for_path)(tmpfilename);
        }
        response_id = GTK_RESPONSE_OK;
      } else {
        response_id = GTK_RESPONSE_REJECT;
        if (perr_message) {
          *perr_message = new char[strlen(tmpfilename)+50];
          snprintf(*perr_message, strlen(tmpfilename)+50, "Can't create temporary file %s", tmpfilename);
        }
      }
    }
  }
  CALL_GTK(g_object_unref)(psettings);
  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_OK) {
    typedef void (*gtk_widget_set_visible_t)(GtkWidget*, gboolean);
    if (pdialog) CALL_GTK(gtk_widget_set_visible)((GtkWidget*)pdialog, false);
    if (Fl_Posix_System_Driver::gtk_major_version < 4) CALL_GTK(gtk_widget_destroy)((GtkWidget*)pdialog);
  }
  if (Fl_Posix_System_Driver::gtk_major_version < 4) {
    while (fl_gtk_events_pending()) fl_gtk_main_iteration();
  } else if (!use_GtkPrintDialog) {
    while (fl_g_main_context_pending(main_context))
      fl_g_main_context_iteration(main_context, false);
  } else if (response_id == GTK_RESPONSE_CANCEL) {
    CALL_GTK(g_object_unref)(dialog414);
  }
  Fl::event_dispatch(old_dispatch);
  Fl_Window *first = Fl::first_window();
  if (first) {
    Fl_Surface_Device::push_current(Fl_Display_Device::display_device());
    first->show();
    while (Fl::ready()) Fl::check();
    Fl_Surface_Device::pop_current();
  }
  if (response_id == GTK_RESPONSE_OK) return 0;
  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) return 1;
  return 2;
}


static void pJobCompleteFunc(Fl_GTK_Printer_Driver::GtkPrintJob *print_job,
                             Fl_GTK_Printer_Driver::gboolean *user_data,
                             const Fl_GTK_Printer_Driver::GError *error) {
  *user_data = true;
}

static void pDestroyNotify(void* data) {}


void Fl_GTK_Printer_Driver::print_file_cb(GtkPrintDialog* dialog, GAsyncResult* res, int *p_response) {
  gboolean b = CALL_GTK(gtk_print_dialog_print_file_finish)(dialog, res, NULL); // 4.14
  *p_response = (b ? GTK_RESPONSE_OK : GTK_RESPONSE_CANCEL);
}


void Fl_GTK_Printer_Driver::end_job() {
  Fl_PDF_File_Surface::end_job();
  if (!pjob && !gfile) return;
  gtk_main_iteration_t fl_gtk_main_iteration = NULL;
  g_main_context_iteration_t fl_g_main_context_iteration = NULL;
  if (Fl_Posix_System_Driver::gtk_major_version >= 4) {
    fl_g_main_context_iteration = CALL_GTK(g_main_context_iteration);
  } else fl_gtk_main_iteration = CALL_GTK(gtk_main_iteration);
  if (pjob) {
    gboolean gb = CALL_GTK(gtk_print_job_set_source_file)(pjob, tmpfilename, NULL); //2.10
    if (gb) {
      gb = false;
      CALL_GTK(gtk_print_job_send)(pjob, (void*)pJobCompleteFunc, &gb, (void*)pDestroyNotify); //2.10
      while (!gb) {
        if (Fl_Posix_System_Driver::gtk_major_version < 4) fl_gtk_main_iteration(); // one iteration of the GTK event loop
        else fl_g_main_context_iteration(main_context, false);
      }
    }
  } else {
    int response_id = GTK_RESPONSE_NONE;
    CALL_GTK(gtk_print_dialog_print_file)(dialog414, NULL, print_setup, gfile, NULL, // 4.14
                                          (GAsyncReadyCallback)print_file_cb, &response_id);
    while (response_id == GTK_RESPONSE_NONE) {
      fl_g_main_context_iteration(main_context, false);
    }
  }
  fl_unlink(tmpfilename);
  if (pjob) CALL_GTK(g_object_unref)(pjob);
  if (print_setup) CALL_GTK(gtk_print_setup_unref)(print_setup);
  if (dialog414) CALL_GTK(g_object_unref)(dialog414);
  if (gfile) CALL_GTK(g_object_unref)(gfile);
}
#endif // HAVE_DLSYM && HAVE_DLFCN_H


Fl_Paged_Device* Fl_Printer::newPrinterDriver(void)
{
#if HAVE_DLSYM && HAVE_DLFCN_H
  static bool gtk = ( Fl::option(Fl::OPTION_PRINTER_USES_GTK) ? Fl_GTK_Printer_Driver::probe_for_GTK() : false);
  if (gtk) return new Fl_GTK_Printer_Driver();
#endif
  return new Fl_Posix_Printer_Driver();
}

/*    Begins a print job. */
int Fl_Posix_Printer_Driver::begin_job(int pages, int *firstpage, int *lastpage, char **perr_message) {
  enum Fl_Paged_Device::Page_Format format;
  enum Fl_Paged_Device::Page_Layout layout;

  // first test version for print dialog
  if (!print_panel) make_print_panel();
  printing_style style = print_load();
  print_selection->deactivate();
  print_all->setonly();
  print_all->do_callback();
  print_from->value("1");
  { char tmp[10]; snprintf(tmp, sizeof(tmp), "%d", pages); print_to->value(tmp); }
  print_panel->show(); // this is modal
  while (print_panel->shown()) Fl::wait();

  if (!print_start) // user clicked cancel
    return 1;

  // get options

  switch (print_page_size->value()) {
    case 0:
      format = Fl_Paged_Device::LETTER;
      break;
    case 2:
      format = Fl_Paged_Device::LEGAL;
      break;
    case 3:
      format = Fl_Paged_Device::EXECUTIVE;
      break;
    case 4:
      format = Fl_Paged_Device::A3;
      break;
    case 5:
      format = Fl_Paged_Device::A5;
      break;
    case 6:
      format = Fl_Paged_Device::B5;
      break;
    case 7:
      format = Fl_Paged_Device::ENVELOPE;
      break;
    case 8:
      format = Fl_Paged_Device::DLE;
      break;
    case 9:
      format = Fl_Paged_Device::TABLOID;
      break;
    default:
      format = Fl_Paged_Device::A4;
  }

  { // page range choice
    int from = 1, to = pages;
    if (print_pages->value()) {
      sscanf(print_from->value(), "%d", &from);
      sscanf(print_to->value(), "%d", &to);
    }
    if (from < 1) from = 1;
    if (to > pages) to = pages;
    if (to < from) to = from;
    if (firstpage) *firstpage = from;
    if (lastpage) *lastpage = to;
    if (pages > 0) pages = to - from + 1;
  }

  if (print_output_mode[0]->value()) layout = Fl_Paged_Device::PORTRAIT;
  else if (print_output_mode[1]->value()) layout = Fl_Paged_Device::LANDSCAPE;
  else if (print_output_mode[2]->value()) layout = Fl_Paged_Device::PORTRAIT;
  else layout = Fl_Paged_Device::LANDSCAPE;

  int print_pipe = print_choice->value();       // 0 = print to file, >0 = printer (pipe)

  const char *media = print_page_size->text(print_page_size->value());
  const char *printer = (const char *)print_choice->menu()[print_choice->value()].user_data();
  if (!print_pipe) printer = "<File>";

  if (!print_pipe) // fall back to file printing
    return Fl_PostScript_File_Device::begin_job (pages, format, layout);

  // Print: pipe the output into the lp command...

  char command[1024];
  if (style == SystemV) snprintf(command, sizeof(command), "lp -s -d %s -n %d -t '%s' -o media=%s",
                                 printer, print_collate_button->value() ? 1 : (int)(print_copies->value() + 0.5), "FLTK", media);
  else snprintf(command, sizeof(command), "lpr -h -P%s -#%d -T FLTK ",
                printer, print_collate_button->value() ? 1 : (int)(print_copies->value() + 0.5));

  Fl_PostScript_Graphics_Driver *ps = driver();
  ps->output = popen(command, "w");
  if (!ps->output) {
    if (perr_message) {
      *perr_message = new char[strlen(command) + 50];
      snprintf(*perr_message, strlen(command) + 50, "could not run command: %s", command);
    }
    return 2;
  }
  ps->close_command(pclose);
  return ps->start_postscript(pages, format, layout); // start printing
}

#endif // !defined(FL_NO_PRINT_SUPPORT)
