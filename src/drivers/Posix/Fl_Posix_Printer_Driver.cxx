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
class Fl_GTK_Printer_Driver : public Fl_PostScript_File_Device {
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

  GtkPrintJob *pjob; // data shared between begin_job() and end_job()
  char tmpfilename[50]; // name of temporary PostScript file containing to-be-printed data
  GMainContext *main_context;
  int begin_job(int pagecount = 0, int *frompage = NULL, int *topage = NULL, char **perr_message=NULL) FL_OVERRIDE;
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
  typedef int (*gtk_printer_accepts_ps_t)(GtkPrinter*);
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
  
  Fl_GTK_Printer_Driver() : Fl_PostScript_File_Device(), main_context(NULL) {}
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

/*static void explore(Fl_GTK_Printer_Driver::GtkWidget *w) {
  typedef Fl_GTK_Printer_Driver::GtkWidget *(*gtk_widget_get_first_child_t)(Fl_GTK_Printer_Driver::GtkWidget*);
  typedef Fl_GTK_Printer_Driver::GtkWidget *(*gtk_widget_get_next_sibling_t)(Fl_GTK_Printer_Driver::GtkWidget*);
  typedef char** (*gtk_widget_get_css_classes_t)(Fl_GTK_Printer_Driver::GtkWidget*);
  w = ((gtk_widget_get_first_child_t)dlsym(Fl_GTK_Printer_Driver::ptr_gtk, "gtk_widget_get_first_child"))(w);
  while (w) {
    char **css = ((gtk_widget_get_css_classes_t)dlsym(Fl_GTK_Printer_Driver::ptr_gtk, "gtk_widget_get_css_classes"))(w);
    while (*css) {puts(*css); css++; }
    explore(w);
    w = ((gtk_widget_get_next_sibling_t)dlsym(Fl_GTK_Printer_Driver::ptr_gtk, "gtk_widget_get_next_sibling"))(w);
  }
}*/

int Fl_GTK_Printer_Driver::begin_job(int pagecount, int *firstpage, int *lastpage, char **perr_message) {
  enum Fl_Paged_Device::Page_Format format = Fl_Paged_Device::A4;
  enum Fl_Paged_Device::Page_Layout layout = Fl_Paged_Device::PORTRAIT ;

  GtkWindow *tmp_win = NULL;
  if (Fl_Posix_System_Driver::gtk_major_version == 4) {
    // Create a temporary GtkWindow and use it as parent of the GtkPrintUnixDialog
    // which avoids message "GtkDialog mapped without a transient parent".
    tmp_win = CALL_GTK(gtk_window_new)();
  }

  GtkPrintUnixDialog *pdialog = CALL_GTK(gtk_print_unix_dialog_new)(Fl_Printer::dialog_title, tmp_win); //2.10
//explore((GtkWidget*)pdialog);
  CALL_GTK(gtk_print_unix_dialog_set_embed_page_setup)(pdialog, true); //2.18
  GtkPrintSettings *psettings = CALL_GTK(gtk_print_unix_dialog_get_settings)(pdialog); //2.10
  CALL_GTK(gtk_print_settings_set)(psettings, "output-file-format", "ps"); //2.10
  char line[FL_PATH_MAX + 20], cwd[FL_PATH_MAX];
  snprintf(line, FL_PATH_MAX + 20, "file://%s/FLTK.ps", fl_getcwd(cwd, FL_PATH_MAX));
  CALL_GTK(gtk_print_settings_set)(psettings, "output-uri", line); //2.10
  CALL_GTK(gtk_print_unix_dialog_set_settings)(pdialog, psettings); //2.10
  CALL_GTK(g_object_unref)(psettings);
  int response_id = GTK_RESPONSE_NONE;
  CALL_GTK(g_signal_connect_data)(pdialog, "response", GCallback(run_response_handler), &response_id, NULL,  0);
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
    CALL_GTK(gtk_window_present)((GtkWindow*)pdialog);
  }
  Fl_Event_Dispatch old_dispatch = Fl::event_dispatch();
  // prevent FLTK from processing any event
  Fl::event_dispatch(no_dispatch);
  //TODO GUI blocks if type esc while print dialog is open in Ubuntu
  while (response_id == GTK_RESPONSE_NONE) { // loop that shows the GTK dialog window
    if (Fl_Posix_System_Driver::gtk_major_version < 4) fl_gtk_main_iteration(); // one iteration of the GTK event loop
    else {
      fl_g_main_context_iteration(main_context, false);
    }
    while (Fl::ready()) Fl::check(); // queued iterations of the FLTK event loop
  }
  if (tmp_win) CALL_GTK(gtk_window_destroy)(tmp_win);
  if (response_id == GTK_RESPONSE_OK) {
    GtkPageSetup *psetup = CALL_GTK(gtk_print_unix_dialog_get_page_setup)(pdialog); //2.10
    GtkPageOrientation orient = CALL_GTK(gtk_page_setup_get_orientation)(psetup); //2.10
    if (orient == GTK_PAGE_ORIENTATION_LANDSCAPE) layout = Fl_Paged_Device::LANDSCAPE;
    GtkPaperSize* psize = CALL_GTK(gtk_page_setup_get_paper_size)(psetup); //2.10
    const char *pname = CALL_GTK(gtk_paper_size_get_name)(psize); //2.10
    if (strcmp(pname, GTK_PAPER_NAME_LETTER) == 0) format = Fl_Paged_Device::LETTER;
    else if (strcmp(pname, GTK_PAPER_NAME_LEGAL) == 0) format = Fl_Paged_Device::LEGAL;
    else if (strcmp(pname, GTK_PAPER_NAME_A3) == 0) format = Fl_Paged_Device::A3;
    else if (strcmp(pname, GTK_PAPER_NAME_A5) == 0) format = Fl_Paged_Device::A5;
    else if (strcmp(pname, GTK_PAPER_NAME_JB5) == 0) format = Fl_Paged_Device::B5;
    else if (strcmp(pname, GTK_PAPER_NAME_TABLOID) == 0) format = Fl_Paged_Device::TABLOID;
    else if (strcmp(pname, GTK_PAPER_NAME_DLE) == 0) format = Fl_Paged_Device::DLE;
    GtkPrinter *gprinter = CALL_GTK(gtk_print_unix_dialog_get_selected_printer)(pdialog); //2.10
    psettings = CALL_GTK(gtk_print_unix_dialog_get_settings)(pdialog); //2.10
    const char* p = CALL_GTK(gtk_print_settings_get)(psettings, "output-uri"); //2.10
    bool printing_to_file = (p != NULL);
    if (printing_to_file) {
      p += 6; // skip "file://" prefix
      strcpy(line, p);
      int l = strlen(p);
      if (strcmp(p+l-4, "/.ps") == 0) {
        line[l-3] = 0;
        strcat(line, "FLTK.ps");
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
      FILE *output = fopen(line, "w");
      if (output) {
        Fl_PostScript_File_Device::begin_job(output, 0, format, layout);
        response_id = GTK_RESPONSE_OK;
      } else {
        response_id = GTK_RESPONSE_REJECT;
        if (perr_message) {
          *perr_message = new char[strlen(line)+50];
          snprintf(*perr_message, strlen(line)+50, "Can't open output file %s", line);
        }
      }
    } else if ( CALL_GTK(gtk_printer_accepts_ps)(gprinter) && //2.10
        CALL_GTK(gtk_printer_is_active)(gprinter) ) { // 2.10
      strcpy(tmpfilename, "/tmp/FLTKprintjobXXXXXX");
      int fd = mkstemp(tmpfilename);
      if (fd >= 0) {
        FILE *output = fdopen(fd, "w");
        Fl_PostScript_File_Device::begin_job(output, 0, format, layout);
        pjob = CALL_GTK(gtk_print_job_new)("FLTK print job", gprinter, psettings, psetup); //2.10
        response_id = GTK_RESPONSE_OK;
      } else {
        response_id = GTK_RESPONSE_REJECT;
        if (perr_message) {
          *perr_message = new char[strlen(tmpfilename)+50];
          snprintf(*perr_message, strlen(tmpfilename)+50, "Can't create temporary file %s", tmpfilename);
        }
      }
    }
    CALL_GTK(g_object_unref)(psettings);
  }
  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_OK) {
    typedef void (*gtk_widget_set_visible_t)(GtkWidget*, gboolean);
    CALL_GTK(gtk_widget_set_visible)((GtkWidget*)pdialog, false);
    if (Fl_Posix_System_Driver::gtk_major_version < 4) CALL_GTK(gtk_widget_destroy)((GtkWidget*)pdialog);
  }
  if (Fl_Posix_System_Driver::gtk_major_version < 4) {
    while (fl_gtk_events_pending()) fl_gtk_main_iteration();
  } else {
    while (fl_g_main_context_pending(main_context))
      fl_g_main_context_iteration(main_context, false);
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

static void pJobCompleteFunc(Fl_GTK_Printer_Driver::GtkPrintJob *print_job, Fl_GTK_Printer_Driver::gboolean *user_data, const Fl_GTK_Printer_Driver::GError *error) {
  *user_data = true;
}
static void pDestroyNotify(void* data) {}

/*static void status_changed_handler(Fl_GTK_Printer_Driver::GtkPrintJob *dialog, void *user_data)
{
  typedef int (*gtk_print_job_get_status_t)(Fl_GTK_Printer_Driver::GtkPrintJob *);
  gtk_print_job_get_status_t f = (gtk_print_job_get_status_t)dlsym(user_data, "gtk_print_job_get_status");
  printf("status=%d\n", f(dialog));
}*/


void Fl_GTK_Printer_Driver::end_job() {
  Fl_PostScript_File_Device::end_job();
  Fl_PostScript_Graphics_Driver *psgd = driver();
  fclose(psgd->output);
  if (!pjob) return;
  GError *gerr;
  gtk_main_iteration_t fl_gtk_main_iteration = NULL;
  g_main_context_iteration_t fl_g_main_context_iteration = NULL;
  if (Fl_Posix_System_Driver::gtk_major_version >= 4) {
    fl_g_main_context_iteration = CALL_GTK(g_main_context_iteration);
  } else fl_gtk_main_iteration = CALL_GTK(gtk_main_iteration);
  //CALL_GTK(g_signal_connect_data)(pjob, "status-changed", GCallback(status_changed_handler), ptr_gtk, NULL,  0);
  gboolean gb = CALL_GTK(gtk_print_job_set_source_file)(pjob, tmpfilename, &gerr); //2.10
  if (gb) {
    gb = false;
    CALL_GTK(gtk_print_job_send)(pjob, (void*)pJobCompleteFunc, &gb, (void*)pDestroyNotify); //2.10
    while (!gb) {
      if (Fl_Posix_System_Driver::gtk_major_version < 4) fl_gtk_main_iteration(); // one iteration of the GTK event loop
      else fl_g_main_context_iteration(main_context, false);
    }
  }
  fl_unlink(tmpfilename);
  CALL_GTK(g_object_unref)(pjob);
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
