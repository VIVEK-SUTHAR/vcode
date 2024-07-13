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

/** Prototypes */
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPromt(char *promt, void (*callback)(char *, int));

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
enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH,
};

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
/** data **/

struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *single_line_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
};

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
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
  int relative_line_no;
  char status_message[80];
  time_t status_message_time;
  struct editorSyntax *syntax;
  struct termios original_termios;
};

struct editorConfig E;

/*** Filetypes ***/
char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {"switch",    "if",      "while",   "for",    "break",
                         "continue",  "return",  "else",    "struct", "union",
                         "typedef",   "static",  "enum",    "class",  "case",
                         "int|",      "long|",   "double|", "float|", "char|",
                         "unsigned|", "signed|", "void|",   NULL};
struct editorSyntax HLDB[] = {{"c", C_HL_extensions, C_HL_keywords, "//", "/*",
                               "*/",
                               HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS}};

#define HLDB_ENTRIED (sizeof(HLDB) / sizeof(HLDB[0]))

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

/* Syntex Highlight  */
int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);
  if (E.syntax == NULL)
    return;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->single_line_comment_start;
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;

  int scslen = scs ? strlen(scs) : 0;
  int mcslen = mcs ? strlen(mcs) : 0;
  int mcelen = mce ? strlen(mce) : 0;

  // To Keep track of whether last character was separator or not;
  int prev_sep = 1;
  // To Keep track of wheter we are in string or not
  int in_string = 0;

  // To Keep track of wheter we are in comment or not
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scslen && !in_string && !in_comment) {

      if (!strncmp(&row->render[i], scs, scslen)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

    if (mcslen && mcelen && !in_string) {
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mcelen)) {
          memset(&row->hl[i], HL_MLCOMMENT, mcelen);
          i += mcelen;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(&row->render[i], mcs, mcslen)) {
        memset(&row->hl[i], HL_MLCOMMENT, mcslen);
        i += mcslen;
        in_comment = 1;
        continue;
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_string) {

        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string)
          in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {

      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep) {

      int j = 0;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2)
          klen--;
        if (!strncmp(&row->render[i], keywords[j], klen) &&
            is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx + 1 < E.numrows)
    editorUpdateSyntax(&E.row[row->idx + 1]);
}
int editorSyntaxToColor(int hl) {
  switch (hl) {
  case HL_MLCOMMENT:
  case HL_COMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_STRING:
    return 35;
  case HL_NUMBER:
    return 31;
  case HL_MATCH:
    return 34;
  default:
    return 37;
  }
}

void editorSelectSyntaxHighLight() {

  E.syntax = NULL;
  if (E.filename == NULL)
    return;
  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIED; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;
        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
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

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (VCODE_TAB_STOP - 1) - (cur_rx % VCODE_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx)
      return cx;
  }
  return cx;
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
  editorUpdateSyntax(row);
}
void editorInsertRow(int at, char *s, size_t len) {

  if (at < 0 || at > E.numrows)
    return;

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = 0; j <= E.numrows; j++)
    E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  ediorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}
void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows)
    return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++)
    E.row[j].idx--;
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
    editorInsertRow(E.numrows, "", 0);
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

void editorInsertNewLine() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else if (E.cx == E.row[E.cy].size + E.numDigits) {
    editorInsertRow(E.cy + 1, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    int len = row->size - (E.cx - E.numDigits);

    editorInsertRow(E.cy + 1, &row->chars[E.cx - E.numDigits], len);
    row = &E.row[E.cy];
    row->size = E.cx - E.numDigits;
    row->chars[row->size] = '\0';
    ediorUpdateRow(row);
  }
  E.cy++;
  E.cx = E.numDigits;
}

void editorRowDelChar(erow *row, int at) {
  at = at - E.numDigits;
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
  if (E.cx == E.numDigits && E.cy == 0)
    return;
  erow *row = &E.row[E.cy];
  // This means we are at the end of Line
  if (E.cx == row->size) {
    if (E.cy < 0)
      return;
    E.cx = E.row[E.cy - 1].size + E.numDigits;
    editorDelRow(E.cy);
    E.cy--;
    return;
  }
  // This means we are at the beginning of line
  if (E.numDigits == E.cx) {
    if (E.cy == 0)
      return;
    E.cx = E.row[E.cy - 1].size + E.numDigits;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
    return;
  }
  if (E.cx > E.numDigits) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    if (E.cy < 0)
      return;
    E.cx = E.row[E.cy - 1].size + E.numDigits;
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

  editorSelectSyntaxHighLight();

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
    editorInsertRow(E.numrows, line, linelen);
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
  if (E.filename == NULL) {
    E.filename = editorPromt("Save as: %s", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighLight();
  }
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

/** find  **/
void editorFindCallBack(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;
  int current = last_match;
  int i = 0;
  for (int i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1)
      current = E.numrows - 1;
    else if (current == E.numrows)
      current = 0;
    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      // Consider Rendering Line no digit also
      E.cx = editorRowRxToCx(row, match - row->render) + E.numDigits;
      E.rowoffset = E.numrows;

      // Restore the Saved highlight
      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}
void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloffset = E.coloffset;
  int saved_rowoffset = E.rowoffset;
  char *query = editorPromt("Search: %s (ESC to cancel)", editorFindCallBack);
  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloffset = saved_coloffset;
    E.rowoffset = saved_rowoffset;
  }
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
      int relative_line_no = abs(filerow - E.cy);
      int linelen = snprintf(lineno, sizeof(lineno), "%*d ", E.numDigits,
                             E.relative_line_no == 0 ? filerow + 1
                                                     : relative_line_no + 1);
      append_bufferAppend(ab, E.cy == filerow ? ANSI_WHITE : ANSI_GRAY, 5);
      append_bufferAppend(ab, lineno, linelen);
      append_bufferAppend(ab, ANSI_RESET, 4);
      int len = E.row[filerow].rsize - E.coloffset;
      if (len < 0)
        len = 0;
      if (len > E.screenCols)
        len = E.screenCols;
      char *c = &E.row[filerow].render[E.coloffset];
      unsigned char *hl = &E.row[filerow].hl[E.coloffset];
      int current_color = -1;

      int j;
      for (j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j <= 26] ? '@' + c[j] : '?');
          append_bufferAppend(ab, "\x1b[7m", 4);
          append_bufferAppend(ab, &sym, 1);
          append_bufferAppend(ab, "\x1b[m", 3);

          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\xab[%dm", current_color);
            append_bufferAppend(ab, buf, clen);
          }

        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            append_bufferAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          append_bufferAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            append_bufferAppend(ab, buf, clen);
          }
          append_bufferAppend(ab, &c[j], 1);
        }
      }
      append_bufferAppend(ab, "\x1b[39m", 5);
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
  int rlen =
      snprintf(rstatus, sizeof(rstatus), "%s %d:%d",
               E.syntax ? E.syntax->filetype : "No filetype", E.cy + 1, E.cx);

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
char *editorPromt(char *promt, void (*callback)(char *, int)) {

  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(promt, buf);
    editorRefreshScreen();
    int c = editorReadKey();
    if (c == DELETE_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback)
        callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback)
          callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback)
      callback(buf, c);
  }
}
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case ARROW_LEFT:
    if (E.cx > E.numDigits) {
      E.cx--;
    } else if (E.cy > 1) {
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
    if (E.cy <= 0)
      E.cy = 0;
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
  int nextKey;
  switch (c) {
  case '\r':
    editorInsertNewLine();
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
  case CTRL_KEY('r'):
    nextKey = editorReadKey();
    if (nextKey == 'n')
      E.relative_line_no = E.relative_line_no == 0 ? 1 : 0;
    break;
  case CTRL_KEY('f'):
    editorFind();
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
  E.relative_line_no = 0;
  E.status_message[0] = '\0';
  E.syntax = NULL;
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
  editorSetStatusMessage(
      "HELP: Ctrl-s = Save File | Ctrl-Q = quit | Ctrl-F = "
      "Find(Use ESC/Arrow/Enter ) | Ctrn-r n= toggle relative line no");
  while (1) {
    editorRefreshScreen();
    editorProcessKeyMap();
  }
  return 0;
}
