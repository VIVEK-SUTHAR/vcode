
/** includes **/
#include "editor.h"
#include "terminal.h"
#include "window.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/_types/_ssize_t.h>
#include <sys/_types/_va_list.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
/** defines **/

#define VCODE_VERSION "0.0.1"
#define VCODE_TAB_STOP 4
#define ANSI_GRAY "\x1b[90m"
#define ANSI_RESET "\x1b[0m"
#define ANSI_WHITE "\x1b[97m"
#define VCODE_QUIT_TIMES 3

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

extern Editor E;

void updateEditotLineNumDigits() {
  int numRows = E.numrows;
  while (numRows >= 10) {
    E.numDigits++;
    numRows /= 10;
  }
  E.screenCols -= E.numDigits;
  E.coloffset -= E.numDigits;
}

void editorOnWidownResize() {
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
    die("OnWindowSize Change");
  }
  E.screenRows -= 2;
  editorRefreshScreen();
}
/** input **/
/** init **/
int main(int argc, char *argv[]) {
  signal(SIGWINCH, editorOnWidownResize);
  enabelRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  editorSetStatusMessage(
      "HELP: Ctrl-s = Save File | Ctrl-Q = quit | Ctrl-F = "
      "Find(Use ESC/Arrow/Enter ) | Ctrn-r n= toggle relative line no");
  while (1) {
    editorRefreshScreen();
    editorProcessKeyMap();
  }
  return 0;
}
