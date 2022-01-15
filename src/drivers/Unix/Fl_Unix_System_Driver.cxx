//
// Definition of Unix/Linux system driver
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

#include "Fl_Unix_System_Driver.H"
#include <FL/Fl_File_Browser.H>
#include <FL/platform.H>
#include "../../flstring.h"

#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>      // errno
#include <sys/time.h>   // gettimeofday()


#ifndef HAVE_SCANDIR
extern "C" {
  int fl_scandir(const char *dirname, struct dirent ***namelist,
                 int (*select)(struct dirent *),
                 int (*compar)(struct dirent **, struct dirent **),
                 char *errmsg, int errmsg_sz);
}
#endif


int Fl_Unix_System_Driver::clocale_printf(FILE *output, const char *format, va_list args) {
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


int Fl_Unix_System_Driver::open_uri(const char *uri, char *msg, int msglen)
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


int Fl_Unix_System_Driver::file_browser_load_filesystem(Fl_File_Browser *browser, char *filename, int lname, Fl_File_Icon *icon)
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

void Fl_Unix_System_Driver::newUUID(char *uuidBuffer)
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

char *Fl_Unix_System_Driver::preference_rootnode(Fl_Preferences *prefs, Fl_Preferences::Root root, const char *vendor,
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

void Fl_Unix_System_Driver::display_arg(const char *arg) {
  Fl::display(arg);
}

//
// Needs some docs
// Returns -1 on error, errmsg will contain OS error if non-NULL.
//
int Fl_Unix_System_Driver::filename_list(const char *d,
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

int Fl_Unix_System_Driver::utf8locale() {
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


// returns pointer to the filename, or null if name ends with '/'
const char *Fl_Unix_System_Driver::filename_name(const char *name) {
  const char *p,*q;
  if (!name) return (0);
  for (p=q=name; *p;) if (*p++ == '/') q = p;
  return q;
}


////////////////////////////////////////////////////////////////
// interface to poll/select call:

#  if USE_POLL

#    include <poll.h>
static pollfd *pollfds = 0;

#  else
#    if HAVE_SYS_SELECT_H
#      include <sys/select.h>
#    endif /* HAVE_SYS_SELECT_H */


static fd_set fdsets[3];
static int maxfd;
#    define POLLIN 1
#    define POLLOUT 4
#    define POLLERR 8

#  endif /* USE_POLL */

static int nfds = 0;
static int fd_array_size = 0;
struct FD {
#  if !USE_POLL
  int fd;
  short events;
#  endif
  void (*cb)(int, void*);
  void* arg;
};

static FD *fd = 0;

void Fl_Unix_System_Driver::add_fd(int n, int events, void (*cb)(int, void*), void *v) {
  remove_fd(n,events);
  int i = nfds++;
  if (i >= fd_array_size) {
    FD *temp;
    fd_array_size = 2*fd_array_size+1;

    if (!fd) temp = (FD*)malloc(fd_array_size*sizeof(FD));
    else temp = (FD*)realloc(fd, fd_array_size*sizeof(FD));

    if (!temp) return;
    fd = temp;

#  if USE_POLL
    pollfd *tpoll;

    if (!pollfds) tpoll = (pollfd*)malloc(fd_array_size*sizeof(pollfd));
    else tpoll = (pollfd*)realloc(pollfds, fd_array_size*sizeof(pollfd));

    if (!tpoll) return;
    pollfds = tpoll;
#  endif
  }
  fd[i].cb = cb;
  fd[i].arg = v;
#  if USE_POLL
  pollfds[i].fd = n;
  pollfds[i].events = events;
#  else
  fd[i].fd = n;
  fd[i].events = events;
  if (events & POLLIN) FD_SET(n, &fdsets[0]);
  if (events & POLLOUT) FD_SET(n, &fdsets[1]);
  if (events & POLLERR) FD_SET(n, &fdsets[2]);
  if (n > maxfd) maxfd = n;
#  endif
}

void Fl_Unix_System_Driver::add_fd(int n, void (*cb)(int, void*), void* v) {
  add_fd(n, POLLIN, cb, v);
}

void Fl_Unix_System_Driver::remove_fd(int n, int events) {
  int i,j;
# if !USE_POLL
  maxfd = -1; // recalculate maxfd on the fly
# endif
  for (i=j=0; i<nfds; i++) {
#  if USE_POLL
    if (pollfds[i].fd == n) {
      int e = pollfds[i].events & ~events;
      if (!e) continue; // if no events left, delete this fd
      pollfds[j].events = e;
    }
#  else
    if (fd[i].fd == n) {
      int e = fd[i].events & ~events;
      if (!e) continue; // if no events left, delete this fd
      fd[i].events = e;
    }
    if (fd[i].fd > maxfd) maxfd = fd[i].fd;
#  endif
    // move it down in the array if necessary:
    if (j<i) {
      fd[j] = fd[i];
#  if USE_POLL
      pollfds[j] = pollfds[i];
#  endif
    }
    j++;
  }
  nfds = j;
#  if !USE_POLL
  if (events & POLLIN) FD_CLR(n, &fdsets[0]);
  if (events & POLLOUT) FD_CLR(n, &fdsets[1]);
  if (events & POLLERR) FD_CLR(n, &fdsets[2]);
#  endif
}

void Fl_Unix_System_Driver::remove_fd(int n) {
  remove_fd(n, -1);
}


// these pointers are set by the Fl::lock() function:
static void nothing() {}
void (*fl_lock_function)() = nothing;
void (*fl_unlock_function)() = nothing;


// This is never called with time_to_wait < 0.0:
// It should return negative on error, 0 if nothing happens before
// timeout, and >0 if any callbacks were done.
int Fl_Unix_System_Driver::poll_or_select_with_delay(double time_to_wait) {
#  if !USE_POLL
    fd_set fdt[3];
    fdt[0] = fdsets[0];
    fdt[1] = fdsets[1];
    fdt[2] = fdsets[2];
#  endif
    int n;
    
    fl_unlock_function();
    
    if (time_to_wait < 2147483.648) {
#  if USE_POLL
      n = ::poll(pollfds, nfds, int(time_to_wait*1000 + .5));
#  else
      timeval t;
      t.tv_sec = int(time_to_wait);
      t.tv_usec = int(1000000 * (time_to_wait-t.tv_sec));
      n = ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],&t);
#  endif
    } else {
#  if USE_POLL
      n = ::poll(pollfds, nfds, -1);
#  else
      n = ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],0);
#  endif
    }
    fl_lock_function();
    
    if (n > 0) {
      for (int i=0; i<nfds; i++) {
#  if USE_POLL
        if (pollfds[i].revents) fd[i].cb(pollfds[i].fd, fd[i].arg);
#  else
        int f = fd[i].fd;
        short revents = 0;
        if (FD_ISSET(f,&fdt[0])) revents |= POLLIN;
        if (FD_ISSET(f,&fdt[1])) revents |= POLLOUT;
        if (FD_ISSET(f,&fdt[2])) revents |= POLLERR;
        if (fd[i].events & revents) fd[i].cb(f, fd[i].arg);
#  endif
      }
    }
    return n;
}

int Fl_Unix_System_Driver::poll_or_select() {
  if (!nfds) return 0; // nothing to select or poll
#  if USE_POLL
  return ::poll(pollfds, nfds, 0);
#  else
  timeval t;
  t.tv_sec = 0;
  t.tv_usec = 0;
  fd_set fdt[3];
  fdt[0] = fdsets[0];
  fdt[1] = fdsets[1];
  fdt[2] = fdsets[2];
  return ::select(maxfd+1,&fdt[0],&fdt[1],&fdt[2],&t);
#  endif
}


//
//  timers
//


////////////////////////////////////////////////////////////////////////
// Timeouts are stored in a sorted list (*first_timeout), so only the
// first one needs to be checked to see if any should be called.
// Allocated, but unused (free) Timeout structs are stored in another
// linked list (*free_timeout).

struct Timeout {
  double time;
  void (*cb)(void*);
  void* arg;
  Timeout* next;
};
static Timeout* first_timeout, *free_timeout;

// I avoid the overhead of getting the current time when we have no
// timeouts by setting this flag instead of getting the time.
// In this case calling elapse_timeouts() does nothing, but records
// the current time, and the next call will actually elapse time.
static char reset_clock = 1;

static void elapse_timeouts() {
  static struct timeval prevclock;
  struct timeval newclock;
  gettimeofday(&newclock, NULL);
  double elapsed = newclock.tv_sec - prevclock.tv_sec +
    (newclock.tv_usec - prevclock.tv_usec)/1000000.0;
  prevclock.tv_sec = newclock.tv_sec;
  prevclock.tv_usec = newclock.tv_usec;
  if (reset_clock) {
    reset_clock = 0;
  } else if (elapsed > 0) {
    for (Timeout* t = first_timeout; t; t = t->next) t->time -= elapsed;
  }
}


// Continuously-adjusted error value, this is a number <= 0 for how late
// we were at calling the last timeout. This appears to make repeat_timeout
// very accurate even when processing takes a significant portion of the
// time interval:
static double missed_timeout_by;


void Fl_Unix_System_Driver::add_timeout(double time, Fl_Timeout_Handler cb, void *argp) {
  elapse_timeouts();
  missed_timeout_by = 0;
  repeat_timeout(time, cb, argp);
}

void Fl_Unix_System_Driver::repeat_timeout(double time, Fl_Timeout_Handler cb, void *argp) {
  time += missed_timeout_by; if (time < -.05) time = 0;
  Timeout* t = free_timeout;
  if (t) {
      free_timeout = t->next;
  } else {
      t = new Timeout;
  }
  t->time = time;
  t->cb = cb;
  t->arg = argp;
  // insert-sort the new timeout:
  Timeout** p = &first_timeout;
  while (*p && (*p)->time <= time) p = &((*p)->next);
  t->next = *p;
  *p = t;
}

/**
  Returns true if the timeout exists and has not been called yet.
*/
int Fl_Unix_System_Driver::has_timeout(Fl_Timeout_Handler cb, void *argp) {
  for (Timeout* t = first_timeout; t; t = t->next)
    if (t->cb == cb && t->arg == argp) return 1;
  return 0;
}

/**
  Removes a timeout callback. It is harmless to remove a timeout
  callback that no longer exists.

  \note This version removes all matching timeouts, not just the first one.
        This may change in the future.
*/
void Fl_Unix_System_Driver::remove_timeout(Fl_Timeout_Handler cb, void *argp) {
  for (Timeout** p = &first_timeout; *p;) {
    Timeout* t = *p;
    if (t->cb == cb && (t->arg == argp || !argp)) {
      *p = t->next;
      t->next = free_timeout;
      free_timeout = t;
    } else {
      p = &(t->next);
    }
  }
}


double Fl_Unix_System_Driver::wait(double time_to_wait)
{
  static char in_idle;

  if (first_timeout) {
    elapse_timeouts();
    Timeout *t;
    while ((t = first_timeout)) {
      if (t->time > 0) break;
      // The first timeout in the array has expired.
      missed_timeout_by = t->time;
      // We must remove timeout from array before doing the callback:
      void (*cb)(void*) = t->cb;
      void *argp = t->arg;
      first_timeout = t->next;
      t->next = free_timeout;
      free_timeout = t;
      // Now it is safe for the callback to do add_timeout:
      cb(argp);
    }
  } else {
    reset_clock = 1; // we are not going to check the clock
  }
  Fl::run_checks();
  if (Fl::idle) {
    if (!in_idle) {
      in_idle = 1;
      Fl::idle();
      in_idle = 0;
    }
    // the idle function may turn off idle, we can then wait:
    if (Fl::idle) time_to_wait = 0.0;
  }
  if (first_timeout && first_timeout->time < time_to_wait)
    time_to_wait = first_timeout->time;
//fprintf(stderr,"time_to_wait=%g\n", time_to_wait);
  static Fl_Unix_System_Driver *sys_dr = (Fl_Unix_System_Driver*)Fl::system_driver();
  if (time_to_wait <= 0.0) {
    // do flush second so that the results of events are visible:
    int ret = sys_dr->poll_or_select_with_delay(0.0);
    Fl::flush();
    return ret;
  } else {
    // do flush first so that user sees the display:
    Fl::flush();
    if (Fl::idle && !in_idle) // 'idle' may have been set within flush()
      time_to_wait = 0.0;
    else if (first_timeout && first_timeout->time < time_to_wait) {
      // another timeout may have been queued within flush(), see STR #3188
      time_to_wait = first_timeout->time >= 0.0 ? first_timeout->time : 0.0;
    }
    return sys_dr->poll_or_select_with_delay(time_to_wait);
  }
}


int Fl_Unix_System_Driver::ready()
{
  if (first_timeout) {
    elapse_timeouts();
    if (first_timeout->time <= 0) return 1;
  } else {
    reset_clock = 1;
  }
  Fl_Unix_System_Driver *sys_dr = (Fl_Unix_System_Driver*)Fl::system_driver();
  return sys_dr->poll_or_select();
}


static void write_short(unsigned char **cp, short i) {
  unsigned char *c = *cp;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF;
  *cp = c;
}

static void write_int(unsigned char **cp, int i) {
  unsigned char *c = *cp;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF; i >>= 8;
  *c++ = i & 0xFF;
  *cp = c;
}


unsigned char *Fl_Unix_System_Driver::create_bmp(const unsigned char *data, int W, int H, int *return_size) {
  int R = ((3*W+3)/4) * 4; // the number of bytes per row, rounded up to multiple of 4
  int s=H*R;
  int fs=14+40+s;
  unsigned char *b=new unsigned char[fs];
  unsigned char *c=b;
  // BMP header
  *c++='B';
  *c++='M';
  write_int(&c,fs);
  write_int(&c,0);
  write_int(&c,14+40);
  // DIB header:
  write_int(&c,40);
  write_int(&c,W);
  write_int(&c,H);
  write_short(&c,1);
  write_short(&c,24);//bits ber pixel
  write_int(&c,0);//RGB
  write_int(&c,s);
  write_int(&c,0);// horizontal resolution
  write_int(&c,0);// vertical resolution
  write_int(&c,0);//number of colors. 0 -> 1<<bits_per_pixel
  write_int(&c,0);
  // Pixel data
  data+=3*W*H;
  for (int y=0;y<H;++y){
    data-=3*W;
    const unsigned char *s=data;
    unsigned char *p=c;
    for (int x=0;x<W;++x){
      *p++=s[2];
      *p++=s[1];
      *p++=s[0];
      s+=3;
    }
    c+=R;
  }
  *return_size = fs;
  return b;
}


static void read_int(uchar *c, int& i) {
  i = *c;
  i |= (*(++c))<<8;
  i |= (*(++c))<<16;
  i |= (*(++c))<<24;
}


// turn BMP image FLTK produced by create_bmp() back to Fl_RGB_Image
Fl_RGB_Image *Fl_Unix_System_Driver::own_bmp_to_RGB(char *bmp) {
  int w, h;
  read_int((uchar*)bmp + 18, w);
  read_int((uchar*)bmp + 22, h);
  int R=((3*w+3)/4) * 4; // the number of bytes per row, rounded up to multiple of 4
  bmp +=  54;
  uchar *data = new uchar[w*h*3];
  uchar *p = data;
  for (int i = h-1; i >= 0; i--) {
    char *s = bmp + i * R;
    for (int j = 0; j < w; j++) {
      *p++=s[2];
      *p++=s[1];
      *p++=s[0];
      s+=3;
    }
  }
  Fl_RGB_Image *img = new Fl_RGB_Image(data, w, h, 3);
  img->alloc_array = 1;
  return img;
}
