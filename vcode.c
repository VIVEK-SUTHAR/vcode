/** includes **/
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <sys/_types/_va_list.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
/** defines **/

#define VCODE_VERSION "0.0.1"
#define VCODE_TAB_STOP 4
#define ANSI_GRAY "\x1b[90m"
#define ANSI_RESET "\x1b[0m"
#define ANSI_WHITE "\x1b[97m"
#define CTRL_KEY(k) ((k) & 0x1f)
#define VCODE_QUIT_TIMES 3

void editorSetStatusMessage(const char *fmt, ...);
enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DELETE_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/** data **/

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

struct termios original_termios;

struct editorConfig {
  int cx, cy;
  int rx;
  int screenRows;
  int screenCols;
  int numrows;
  int numDigits;
  erow *row;
  int dirty;
  int rowoffset;
  int coloffset;
  char *filename;
  char status_message[80];
  time_t status_message_time;
  struct termios original_termios;
};

struct editorConfig E;

void updateEditotLineNumDigits() {
  int numRows = E.numrows;
  while (numRows >= 10) {
    E.numDigits++;
    numRows /= 10;
  }
  E.screenCols -= E.numDigits;
  E.coloffset -= E.numDigits;
}
/** terminal **/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}
void disablerawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1)
    die("tcsetattr");
}

void enabelRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1)
    die("tcgetattr");
  atexit(disablerawMode);
  struct termios raw = E.original_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  // Arrow Keys Send Escape Seq lile \x1b folloed by [A...D
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';

    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DELETE_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;

  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
  }
  editorReadKey();
  return -1;
}
int getWindowSize(int *rows, int *cols) {
  struct winsize window_size;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1 ||
      window_size.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = window_size.ws_col;
    *rows = window_size.ws_row;
    return 0;
  }
}
/*Row Operations   */

int editorRowCxtoRx(erow *row, int cx) {

  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t') {
      rx += (VCODE_TAB_STOP - 1) - (rx % VCODE_TAB_STOP);
    }
    rx++;
  }
  return rx;
}
void ediorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);
  row->render = malloc(row->size + tabs * (VCODE_TAB_STOP - 1) + 1);

  int index = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[index++] = ' ';
      while (index % VCODE_TAB_STOP != 0) {
        row->render[index++] = ' ';
      }
    } else {
      row->render[index++] = row->chars[j];
    }
  }
  row->render[index] = '\0';
  row->rsize = index;
}
void editorAppendRow(char *s, size_t len) {

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  int at = E.numrows;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  ediorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}
void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows)
    return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  // Conside The Space We added for Rendering Line Numbers
  // Ex If I am at row 10 and want to insert at col 100
  // And the file has 10 Lines so Rendering Space for line digits is 2
  // initial at = 10; But that contains that 2 line digit also
  // So we substract it,so becomes 8 and we insert it 8
  at = at - E.numDigits;
  // Check for not go below 0 or if its last char if its just assign max size
  if (at < 0 || at > row->size)
    at = row->size;
  // reallocate memory to row chars ,2 Extra for new char and null terminator
  // '\0'
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  ediorUpdateRow(row);
  E.dirty++;
}
/** Editor operations*/
void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}
void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  ediorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  at = at - E.numDigits;
  editorSetStatusMessage("%d size", at);
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  ediorUpdateRow(row);
  E.dirty++;
}
void editorDelChar() {
  if (E.cy == E.numrows)
    return;
  if (E.cx == 0 && E.cy == 0)
    return;
  erow *row = &E.row[E.cy];
  if (E.numDigits == E.cx && row->size == 0) {
    if (E.cy < 0)
      return;
    E.cx = E.row[E.cy - 1].size + E.numDigits + 1;
    editorDelRow(E.cy);
    E.cy--;
    return;
  }
  if (E.numDigits == E.cx) {
    E.cx -= E.numDigits;
  }
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    if (E.cy < 0)
      return;
    E.cx = E.row[E.cy - 1].size + 1;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}
int countDigits(int number) {
  int count = 0;
  if (number == 0) {
    return 1;
  }
  while (number != 0) {
    number /= 10;
    count++;
  }
  return count;
}
/** FILE / IO**/

char *editorRowsToString(int *bufferlen) {
  int totelen = 0;
  int j = 0;
  for (j = 0; j < E.numrows; j++)
    totelen += E.row[j].size + 1;
  *bufferlen = totelen;
  char *buffer = malloc(totelen);
  char *p = buffer;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buffer;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  int line_count = 0;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
    line_count++;
  }
  int digits = countDigits(line_count);
  E.numDigits = digits;
  free(line);
  fclose(fp);
  E.cx = 0;
  E.dirty = 0;
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.status_message, sizeof(E.status_message), fmt, ap);
  va_end(ap);
  E.status_message_time = time(NULL);
}
void editorSave() {
  if (E.filename == NULL)
    return;
  int len;
  char *buf = editorRowsToString(&len);
  int fileDescrptior = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fileDescrptior != -1) {
    if (ftruncate(fileDescrptior, len) != -1) {
      if (write(fileDescrptior, buf, len) != -1) {
        close(fileDescrptior);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%s %dL, %d bytes written", E.filename,
                               E.numrows, len);
        return;
      }
    }
    close(fileDescrptior);
  }
  free(buf);
  editorSetStatusMessage("Failed to save file %s", strerror(errno));
}

/** append buffer **/
struct abuf {
  char *b;
  int len;
};
#define ABUF_INIT                                                              \
  { NULL, 0 }

void append_bufferAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
void append_bufferFree(struct abuf *ab) { free(ab->b); }
/** output **/

void editorScroll() {
  E.rx = 0;

  if (E.cy < E.numrows) {
    E.rx = editorRowCxtoRx(&E.row[E.cy], E.cx - E.numDigits) + E.numDigits;
  }
  if (E.cy < E.rowoffset) {
    E.rowoffset = E.cy;
  }
  if (E.cy >= E.rowoffset + E.screenRows) {
    E.rowoffset = E.cy - E.screenRows + 1;
  }

  if (E.rx < E.coloffset + E.numDigits) {
    E.coloffset = E.rx - E.numDigits;
  }
  if (E.rx >= E.coloffset + E.screenCols) {
    E.coloffset = E.rx - E.screenCols + 1;
  }
}
void editorDrawRows(struct abuf *ab) {
  int y;

  for (y = 0; y < E.screenRows; y++) {
    int filerow = y + E.rowoffset;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenRows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "VCode editor -- version %s", VCODE_VERSION);
        if (welcomelen > E.screenCols)
          welcomelen = E.screenCols;
        int padding = (E.screenCols - welcomelen) / 2;
        if (padding) {
          append_bufferAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          append_bufferAppend(ab, " ", 1);
        append_bufferAppend(ab, welcome, welcomelen);
      } else {
        append_bufferAppend(ab, "~", 1);
      }
    } else {
      char lineno[16];
      int linelen =
          snprintf(lineno, sizeof(lineno), "%*d ", E.numDigits, filerow + 1);
      append_bufferAppend(ab, E.cy == filerow ? ANSI_WHITE : ANSI_GRAY, 5);
      append_bufferAppend(ab, lineno, linelen);
      append_bufferAppend(ab, ANSI_RESET, 4);
      int len = E.row[filerow].rsize - E.coloffset;
      if (len < 0)
        len = 0;
      if (len > E.screenCols)
        len = E.screenCols;
      append_bufferAppend(ab, &E.row[filerow].render[E.coloffset], len);
    }

    append_bufferAppend(ab, "\x1b[K", 3);
    append_bufferAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {

  append_bufferAppend(ab, "\x1b[7m", 4);

  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     E.filename ? E.filename : "[No name]", E.numrows,
                     E.dirty ? "Modified" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d:%d", E.cy + 1, E.cx);

  if (len > E.screenCols)
    len = E.screenCols;
  append_bufferAppend(ab, status, len);
  while (len < E.screenCols) {
    if (E.screenCols - len == rlen) {
      append_bufferAppend(ab, rstatus, rlen);
      break;
    } else {
      append_bufferAppend(ab, " ", 1);
      len++;
    }
  }

  append_bufferAppend(ab, "\x1b[m", 3);

  append_bufferAppend(ab, "\r\n", 3);
}

void editorDrawStatusMessageBar(struct abuf *ab) {
  append_bufferAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.status_message);
  if (msglen > E.screenCols)
    msglen = E.screenCols;
  if (msglen && time(NULL) - E.status_message_time < 5) {
    append_bufferAppend(ab, E.status_message, msglen);
  }
}

void editorRefreshScreen() {
  // updateEditotLineNumDigits();
  editorScroll();
  struct abuf ab = ABUF_INIT;

  append_bufferAppend(&ab, "\x1b[?25l", 6);
  append_bufferAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawStatusMessageBar(&ab);
  char buf[32];

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoffset) + 1,
           (E.rx - E.coloffset) + 1);
  append_bufferAppend(&ab, buf, strlen(buf));

  append_bufferAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  append_bufferFree(&ab);
}

void editorOnWidownResize() {
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
    die("OnWindowSize Change");
  }
  E.screenRows -= 2;
  editorRefreshScreen();
}
/** input **/
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case ARROW_LEFT:
    if (E.cx > E.numDigits) {
      E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      row = &E.row[E.cy];
      E.cx = row->size + E.numDigits;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size + E.numDigits) {
      E.cx++;
    } else if (row && E.cx == row->size + E.numDigits) {
      if (E.cy == E.numrows - 1)
        break;
      E.cy++;
      E.cx = E.numDigits;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0)
      E.cy--;

    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows - 1)
      E.cy++;
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlength = row ? row->size + E.numDigits : E.numDigits;
  if (E.cx > rowlength)
    E.cx = rowlength;
  else if (E.cx < E.numDigits) {
    E.cx = E.numDigits;
  }
}
void editorProcessKeyMap() {
  static int quit_time = VCODE_QUIT_TIMES;
  int c = editorReadKey();
  switch (c) {
  case '\r':
    break;
  case CTRL_KEY('q'):
    if (E.dirty && quit_time > 0) {
      editorSetStatusMessage("! File has unsaved changes. "
                             "Press Ctrl-Q %d more times to quit.",
                             quit_time);
      quit_time--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  case CTRL_KEY('s'):
    editorSave();
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DELETE_KEY:
    if (c == DELETE_KEY)
      editorMoveCursor(ARROW_RIGHT);
    editorDelChar();
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      E.cy = E.rowoffset;
    } else if (c == PAGE_DOWN) {
      E.cy = E.rowoffset + E.screenRows - 1;
      if (E.cy > E.numrows)
        E.cy = E.numrows;
    }
    int times = E.screenRows;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;
  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    if (E.cy < E.numrows)
      E.cx = E.row[E.cy].size;
    break;
  case ARROW_DOWN:
  case ARROW_UP:
  case ARROW_RIGHT:
  case ARROW_LEFT:
    editorMoveCursor(c);
    break;
  case CTRL_KEY('l'):
  case '\x1b':
    break;
  default:
    editorInsertChar(c);
    break;
  }
  quit_time = VCODE_QUIT_TIMES;
}
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoffset = 0;
  E.coloffset = 0;
  E.numrows = 0;
  E.numDigits = 1;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.status_message[0] = '\0';
  E.status_message_time = 0;

  if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
    die("getWindowSize");
  E.screenRows -= 2;
}

/** init **/
int main(int argc, char *argv[]) {
  signal(SIGWINCH, editorOnWidownResize);
  enabelRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  editorSetStatusMessage("HELP: Ctrl-s = Save File | Ctrl-Q = quit");
  while (1) {
    editorRefreshScreen();
    editorProcessKeyMap();
  }
  return 0;
}
