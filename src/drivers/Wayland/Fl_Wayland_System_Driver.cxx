//
// Definition of Wayland system driver
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2010-2021 by Bill Spitzak and others.
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

#include "Fl_Wayland_System_Driver.H"
#include <FL/Fl_File_Browser.H>
#include <FL/fl_string.h>  // fl_strdup
#include <FL/platform.H>
#include "../../flstring.h"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "xdg-shell-client-protocol.h"

#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>     // strerror(errno)
#include <errno.h>      // errno
#if HAVE_DLSYM && HAVE_DLFCN_H
#include <dlfcn.h>   // for dlsym
#endif


#ifndef HAVE_SCANDIR
extern "C" {
  int fl_scandir(const char *dirname, struct dirent ***namelist,
                 int (*select)(struct dirent *),
                 int (*compar)(struct dirent **, struct dirent **),
                 char *errmsg, int errmsg_sz);
}
#endif


/**
 Creates a driver that manages all system related calls.

 This function must be implemented once for every platform.
 */
Fl_System_Driver *Fl_System_Driver::newSystemDriver()
{
  return new Fl_Wayland_System_Driver();
}


int Fl_Wayland_System_Driver::clocale_printf(FILE *output, const char *format, va_list args) {
#if defined(__linux__) && defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700
  static locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", duplocale(LC_GLOBAL_LOCALE));
  locale_t previous_locale = uselocale(c_locale);
  int retval = vfprintf(output, format, args);
  uselocale(previous_locale);
#else
  char *saved_locale = setlocale(LC_NUMERIC, NULL);
  setlocale(LC_NUMERIC, "C");
  int retval = vfprintf(output, format, args);
  setlocale(LC_NUMERIC, saved_locale);
#endif
  return retval;
}


// Find a program in the path...
static char *path_find(const char *program, char *filename, int filesize) {
  const char    *path;                  // Search path
  char          *ptr,                   // Pointer into filename
                *end;                   // End of filename buffer


  if ((path = fl_getenv("PATH")) == NULL) path = "/bin:/usr/bin";

  for (ptr = filename, end = filename + filesize - 1; *path; path ++) {
    if (*path == ':') {
      if (ptr > filename && ptr[-1] != '/' && ptr < end) *ptr++ = '/';

      strlcpy(ptr, program, end - ptr + 1);

      if (!access(filename, X_OK)) return filename;

      ptr = filename;
    } else if (ptr < end) *ptr++ = *path;
  }

  if (ptr > filename) {
    if (ptr[-1] != '/' && ptr < end) *ptr++ = '/';

    strlcpy(ptr, program, end - ptr + 1);

    if (!access(filename, X_OK)) return filename;
  }

  return 0;
}


int Fl_Wayland_System_Driver::open_uri(const char *uri, char *msg, int msglen)
{
  // Run any of several well-known commands to open the URI.
  //
  // We give preference to the Portland group's xdg-utils
  // programs which run the user's preferred web browser, etc.
  // based on the current desktop environment in use.  We fall
  // back on older standards and then finally test popular programs
  // until we find one we can use.
  //
  // Note that we specifically do not support the MAILER and
  // BROWSER environment variables because we have no idea whether
  // we need to run the listed commands in a terminal program.
  char  command[FL_PATH_MAX],           // Command to run...
  *argv[4],                     // Command-line arguments
  remote[1024];                 // Remote-mode command...
  const char * const *commands;         // Array of commands to check...
  int i;
  static const char * const browsers[] = {
    "xdg-open", // Portland
    "htmlview", // Freedesktop.org
    "firefox",
    "mozilla",
    "netscape",
    "konqueror", // KDE
    "opera",
    "hotjava", // Solaris
    "mosaic",
    NULL
  };
  static const char * const readers[] = {
    "xdg-email", // Portland
    "thunderbird",
    "mozilla",
    "netscape",
    "evolution", // GNOME
    "kmailservice", // KDE
    NULL
  };
  static const char * const managers[] = {
    "xdg-open", // Portland
    "fm", // IRIX
    "dtaction", // CDE
    "nautilus", // GNOME
    "konqueror", // KDE
    NULL
  };

  // Figure out which commands to check for...
  if (!strncmp(uri, "file://", 7)) commands = managers;
  else if (!strncmp(uri, "mailto:", 7) ||
           !strncmp(uri, "news:", 5)) commands = readers;
  else commands = browsers;

  // Find the command to run...
  for (i = 0; commands[i]; i ++)
    if (path_find(commands[i], command, sizeof(command))) break;

  if (!commands[i]) {
    if (msg) {
      snprintf(msg, msglen, "No helper application found for \"%s\"", uri);
    }

    return 0;
  }

  // Handle command-specific arguments...
  argv[0] = (char *)commands[i];

  if (!strcmp(commands[i], "firefox") ||
      !strcmp(commands[i], "mozilla") ||
      !strcmp(commands[i], "netscape") ||
      !strcmp(commands[i], "thunderbird")) {
    // program -remote openURL(uri)
    snprintf(remote, sizeof(remote), "openURL(%s)", uri);

    argv[1] = (char *)"-remote";
    argv[2] = remote;
    argv[3] = 0;
  } else if (!strcmp(commands[i], "dtaction")) {
    // dtaction open uri
    argv[1] = (char *)"open";
    argv[2] = (char *)uri;
    argv[3] = 0;
  } else {
    // program uri
    argv[1] = (char *)uri;
    argv[2] = 0;
  }

  if (msg) {
    strlcpy(msg, argv[0], msglen);

    for (i = 1; argv[i]; i ++) {
      strlcat(msg, " ", msglen);
      strlcat(msg, argv[i], msglen);
    }
  }

  return run_program(command, argv, msg, msglen) != 0;
}


int Fl_Wayland_System_Driver::file_browser_load_filesystem(Fl_File_Browser *browser, char *filename, int lname, Fl_File_Icon *icon)
{
  int num_files = 0;
  //
  // UNIX code uses /etc/fstab or similar...
  //
  FILE  *mtab;          // /etc/mtab or /etc/mnttab file
  char  line[FL_PATH_MAX];      // Input line

  // Every Unix has a root filesystem '/'.
  // This ensures that the user don't get an empty
  // window after requesting filesystem list.
  browser->add("/", icon);
  num_files ++;

  //
  // Open the file that contains a list of mounted filesystems...
  //
  // Note: this misses automounted filesystems on FreeBSD if absent from /etc/fstab
  //

  mtab = fopen("/etc/mnttab", "r");     // Fairly standard
  if (mtab == NULL)
    mtab = fopen("/etc/mtab", "r");     // More standard
  if (mtab == NULL)
    mtab = fopen("/etc/fstab", "r");    // Otherwise fallback to full list
  if (mtab == NULL)
    mtab = fopen("/etc/vfstab", "r");   // Alternate full list file

  if (mtab != NULL)
  {
    while (fgets(line, sizeof(line), mtab) != NULL)
    {
      if (line[0] == '#' || line[0] == '\n')
        continue;
      if (sscanf(line, "%*s%4095s", filename) != 1)
        continue;
      if (strcmp("/", filename) == 0)
        continue; // "/" was added before

      // Add a trailing slash (except for the root filesystem)
      strlcat(filename, "/", lname);

      //        printf("Fl_File_Browser::load() - adding \"%s\" to list...\n", filename);
      browser->add(filename, icon);
      num_files ++;
    }

    fclose(mtab);
  }
  return num_files;
}

void Fl_Wayland_System_Driver::newUUID(char *uuidBuffer)
{
  unsigned char b[16];
#if HAVE_DLSYM && HAVE_DLFCN_H
  typedef void (*gener_f_type)(uchar*);
  static bool looked_for_uuid_generate = false;
  static gener_f_type uuid_generate_f = NULL;
  if (!looked_for_uuid_generate) {
    looked_for_uuid_generate = true;
    uuid_generate_f = (gener_f_type)dlopen_or_dlsym("libuuid", "uuid_generate");
  }
  if (uuid_generate_f) {
    uuid_generate_f(b);
  } else
#endif
  {
    time_t t = time(0);                   // first 4 byte
    b[0] = (unsigned char)t;
    b[1] = (unsigned char)(t>>8);
    b[2] = (unsigned char)(t>>16);
    b[3] = (unsigned char)(t>>24);
    int r = rand();                       // four more bytes
    b[4] = (unsigned char)r;
    b[5] = (unsigned char)(r>>8);
    b[6] = (unsigned char)(r>>16);
    b[7] = (unsigned char)(r>>24);
    unsigned long a = (unsigned long)&t;  // four more bytes
    b[8] = (unsigned char)a;
    b[9] = (unsigned char)(a>>8);
    b[10] = (unsigned char)(a>>16);
    b[11] = (unsigned char)(a>>24);
    // Now we try to find 4 more "random" bytes. We extract the
    // lower 4 bytes from the address of t - it is created on the
    // stack so *might* be in a different place each time...
    // This is now done via a union to make it compile OK on 64-bit systems.
    union { void *pv; unsigned char a[sizeof(void*)]; } v;
    v.pv = (void *)(&t);
    // NOTE: May need to handle big- or little-endian systems here
# if WORDS_BIGENDIAN
    b[8] = v.a[sizeof(void*) - 1];
    b[9] = v.a[sizeof(void*) - 2];
    b[10] = v.a[sizeof(void*) - 3];
    b[11] = v.a[sizeof(void*) - 4];
# else // data ordered for a little-endian system
    b[8] = v.a[0];
    b[9] = v.a[1];
    b[10] = v.a[2];
    b[11] = v.a[3];
# endif
    char name[80];                        // last four bytes
    gethostname(name, 79);
    memcpy(b+12, name, 4);
  }
  sprintf(uuidBuffer, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
          b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
          b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

char *Fl_Wayland_System_Driver::preference_rootnode(Fl_Preferences *prefs, Fl_Preferences::Root root, const char *vendor,
                                                const char *application)
{
  static char *filename = 0L;
  if (!filename) filename = (char*)::calloc(1, FL_PATH_MAX);
  const char *e;
  switch (root&Fl_Preferences::ROOT_MASK) {
    case Fl_Preferences::USER:
      e = getenv("HOME");
      // make sure that $HOME is set to an existing directory
      if ( (e==0L) || (e[0]==0) || (::access(e, F_OK)==-1) ) {
        struct passwd *pw = getpwuid(getuid());
        e = pw->pw_dir;
      }
      if ( (e==0L) || (e[0]==0) || (::access(e, F_OK)==-1) ) {
        return 0L;
      } else {
        strlcpy(filename, e, FL_PATH_MAX);
        if (filename[strlen(filename)-1] != '/')
          strlcat(filename, "/", FL_PATH_MAX);
        strlcat(filename, ".fltk/", FL_PATH_MAX);
      }
      break;
    case Fl_Preferences::SYSTEM:
      strcpy(filename, "/etc/fltk/");
      break;
  }

  // Make sure that the parameters are not NULL
  if ( (vendor==0L) || (vendor[0]==0) )
    vendor = "unknown";
  if ( (application==0L) || (application[0]==0) )
    application = "unknown";

  snprintf(filename + strlen(filename), FL_PATH_MAX - strlen(filename),
           "%s/%s.prefs", vendor, application);
  return filename;
}

void Fl_Wayland_System_Driver::display_arg(const char *arg) {
  Fl::display(arg);
}

int Fl_Wayland_System_Driver::XParseGeometry(const char* string, int* x, int* y,
                                         unsigned int* width, unsigned int* height) {
  return 0;//::XParseGeometry(string, x, y, width, height);
}

//
// Needs some docs
// Returns -1 on error, errmsg will contain OS error if non-NULL.
//
int Fl_Wayland_System_Driver::filename_list(const char *d,
                                        dirent ***list,
                                        int (*sort)(struct dirent **, struct dirent **),
                                        char *errmsg, int errmsg_sz) {
  int dirlen;
  char *dirloc;

  if (errmsg && errmsg_sz>0) errmsg[0] = '\0';

  // Assume that locale encoding is no less dense than UTF-8
  dirlen = strlen(d);
  dirloc = (char *)malloc(dirlen + 1);
  fl_utf8to_mb(d, dirlen, dirloc, dirlen + 1);

#ifndef HAVE_SCANDIR
  // This version is when we define our own scandir. Note it updates errmsg on errors.
  int n = fl_scandir(dirloc, list, 0, sort, errmsg, errmsg_sz);
#elif defined(HAVE_SCANDIR_POSIX)
  // POSIX (2008) defines the comparison function like this:
  int n = scandir(dirloc, list, 0, (int(*)(const dirent **, const dirent **))sort);
#else
  // The vast majority of UNIX systems want the sort function to have this
  // prototype, most likely so that it can be passed to qsort without any
  // changes:
  int n = scandir(dirloc, list, 0, (int(*)(const void*,const void*))sort);
#endif

  free(dirloc);

  if (n==-1) {
    // Don't write to errmsg if FLTK's fl_scandir() already set it.
    // If OS's scandir() was used (HAVE_SCANDIR), we return its error in errmsg here..
#ifdef HAVE_SCANDIR
    if (errmsg) fl_snprintf(errmsg, errmsg_sz, "%s", strerror(errno));
#endif
    return -1;
  }

  // convert every filename to UTF-8, and append a '/' to all
  // filenames that are directories
  int i;
  char *fullname = (char*)malloc(dirlen+FL_PATH_MAX+3); // Add enough extra for two /'s and a nul
  // Use memcpy for speed since we already know the length of the string...
  memcpy(fullname, d, dirlen+1);

  char *name = fullname + dirlen;
  if (name!=fullname && name[-1]!='/')
    *name++ = '/';

  for (i=0; i<n; i++) {
    int newlen;
    dirent *de = (*list)[i];
    int len = strlen(de->d_name);
    newlen = fl_utf8from_mb(NULL, 0, de->d_name, len);
    dirent *newde = (dirent*)malloc(de->d_name - (char*)de + newlen + 2); // Add space for a / and a nul

    // Conversion to UTF-8
    memcpy(newde, de, de->d_name - (char*)de);
    fl_utf8from_mb(newde->d_name, newlen + 1, de->d_name, len);

    // Check if dir (checks done on "old" name as we need to interact with
    // the underlying OS)
    if (de->d_name[len-1]!='/' && len<=FL_PATH_MAX) {
      // Use memcpy for speed since we already know the length of the string...
      memcpy(name, de->d_name, len+1);
      if (fl_filename_isdir(fullname)) {
        char *dst = newde->d_name + newlen;
        *dst++ = '/';
        *dst = 0;
      }
    }

    free(de);
    (*list)[i] = newde;
  }
  free(fullname);

  return n;
}

int Fl_Wayland_System_Driver::utf8locale() {
  static int ret = 2;
  if (ret == 2) {
    char* s;
    ret = 1; /* assume UTF-8 if no locale */
    if (((s = getenv("LC_CTYPE")) && *s) ||
        ((s = getenv("LC_ALL"))   && *s) ||
        ((s = getenv("LANG"))     && *s)) {
      ret = (strstr(s,"utf") || strstr(s,"UTF"));
    }
  }
  return ret;
}


void Fl_Wayland_System_Driver::own_colormap() {
  fl_open_display();
}


void Fl_Wayland_System_Driver::make_transient(void *ptr_gtk, void *gtk_window, Fl_Window *win) {
  //TODO
  typedef struct _GdkDrawable GdkWindow;
  typedef struct _GtkWidget GtkWidget;

  typedef GdkWindow* (*XX_gtk_widget_get_window_type)(GtkWidget *);
  static XX_gtk_widget_get_window_type fl_gtk_widget_get_window = NULL;

  typedef struct wl_surface *(*XX_gdk_wayland_window_get_wl_surface_type)(GdkWindow *);
  static XX_gdk_wayland_window_get_wl_surface_type fl_gdk_wayland_window_get_wl_surface = NULL;

  if (!fl_gtk_widget_get_window) {
    fl_gtk_widget_get_window = (XX_gtk_widget_get_window_type)dlsym(ptr_gtk, "gtk_widget_get_window");
    if (!fl_gtk_widget_get_window) return;
  }

  GdkWindow* gdkw = fl_gtk_widget_get_window((GtkWidget*)gtk_window);

  if (!fl_gdk_wayland_window_get_wl_surface) {
    fl_gdk_wayland_window_get_wl_surface = (XX_gdk_wayland_window_get_wl_surface_type)dlsym(ptr_gtk, "gdk_wayland_window_get_wl_surface");
  }
  struct wl_surface *wld_surf = fl_gdk_wayland_window_get_wl_surface(gdkw);
fprintf(stderr, "wld_surf=%p \n", wld_surf);
  // but what next?
/* not good:
Fl_Wayland_Screen_Driver *scr_driver = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
struct xdg_surface *xdgs = xdg_wm_base_get_xdg_surface(scr_driver->xdg_wm_base, wld_surf);
struct xdg_toplevel *top = xdg_surface_get_toplevel(xdgs);
fprintf(stderr, "wld_surf=%p xdgs=%p top=%p \n", wld_surf, xdgs, top);
xdg_toplevel_set_parent(top, fl_xid(win)->xdg_toplevel);
*/
}


char *Fl_Wayland_System_Driver::get_prog_name() {
  pid_t pid = getpid();
  char fname[100];
  sprintf(fname, "/proc/%u/cmdline", pid);
  FILE *in = fopen(fname, "r");
  if (in) {
    static char line[200];
    char *p = fgets(line, sizeof(line), in);
    fclose(in);
    p = strrchr(line, '/'); if (!p) p = line; else p++;
    return p;
  }
  return NULL;
}

