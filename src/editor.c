#include "editor.h"
#include "highlight.h"
#include "row.h"
#include "search.h"
#include "string_buffer.h"
#include "terminal.h"
#include "window.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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

Editor E;
EditorGitConfig EGC;

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
  E.current_mode = MODE_INSERT;
  E.status_message_time = 0;
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1)
    die("getWindowSize");
  E.screenRows -= 2;
}

char *get_git_branch() {
  FILE *fp;
  char path[1035];
  char *branch_name = NULL;

  fp = popen("git rev-parse --abbrev-ref HEAD", "r");
  if (fp == NULL) {
    return NULL;
  }

  if (fgets(path, sizeof(path), fp) != NULL) {
    path[strcspn(path, "\n")] = '\0';
    printf("Current branch: %s\n", path);
    branch_name = strdup(path);
  } else {
    printf("Failed to get current branch name\n");
  }

  pclose(fp);
  return branch_name;
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
  char *current_branch = get_git_branch();
  if (current_branch != NULL) {
    EGC.currentGitBranch = current_branch;
  }
  E.cx = 0;
  E.dirty = 0;
}

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

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

void editorDrawRows(struct string_buffer *sb) {
  int y;

  for (y = 0; y < E.screenRows; y++) {
    int filerow = y + E.rowoffset;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenRows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome), "%s -- version %s",
                                  "Welcome to VCode", VCODE_VERSION);
        if (welcomelen > E.screenCols)
          welcomelen = E.screenCols;
        int padding = (E.screenCols - welcomelen) / 2;
        if (padding) {
          string_buffer_append(sb, "~", 1);
          padding--;
        }
        while (padding--)
          string_buffer_append(sb, " ", 1);
        string_buffer_append(sb, welcome, welcomelen);
      } else {
        string_buffer_append(sb, "~", 1);
      }
    } else {
      char lineno[16];
      int relative_line_no = abs(filerow - E.cy);
      int linelen = snprintf(lineno, sizeof(lineno), "%*d ", E.numDigits,
                             E.relative_line_no == 0 ? filerow + 1
                                                     : relative_line_no + 1);
      string_buffer_append(sb, E.cy == filerow ? ANSI_WHITE : ANSI_GRAY, 5);
      string_buffer_append(sb, lineno, linelen);
      string_buffer_append(sb, ANSI_RESET, 4);
      int len = E.row[filerow].rsize - E.coloffset;
      if (len < 0)
        len = 0;
      if (len > E.screenCols)
        len = E.screenCols;
      char *c = &E.row[filerow].render[E.coloffset];
      unsigned char *hl = &E.row[filerow].hl[E.coloffset];
      int current_color = -1;
      if (E.cy == filerow && E.row[E.cy].selectCurrent == 1) {
        string_buffer_append(sb, SELECT_START, strlen(SELECT_START));
      }
      int j;
      for (j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j <= 26] ? '@' + c[j] : '?');
          string_buffer_append(sb, "\x1b[7m", 4);
          string_buffer_append(sb, &sym, 1);
          string_buffer_append(sb, "\x1b[m", 3);

          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\xab[%dm", current_color);
            string_buffer_append(sb, buf, clen);
          }

        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            string_buffer_append(sb, "\x1b[39m", 5);
            current_color = -1;
          }
          string_buffer_append(sb, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            string_buffer_append(sb, buf, clen);
          }
          string_buffer_append(sb, &c[j], 1);
        }
      }

      if (E.row[E.cy].selectCurrent == 1) {
        string_buffer_append(sb, SELECT_END, strlen(SELECT_END));
      }
      string_buffer_append(sb, "\x1b[39m", 5);
    }

    string_buffer_append(sb, "\x1b[K", 3);
    string_buffer_append(sb, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct string_buffer *sb) {
  // Revert the colors
  string_buffer_append(sb, "\x1b[7m", 4);

  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%s %.20s - %d lines %s %s",
                     E.current_mode == 1 ? "INSERT" : "NORMAL",
                     E.filename ? E.filename : "[No name]", E.numrows,
                     E.dirty ? "Modified" : "",
                     EGC.currentGitBranch ? EGC.currentGitBranch : "");
  int rlen =
      snprintf(rstatus, sizeof(rstatus), "%s %d:%d",
               E.syntax ? E.syntax->filetype : "No filetype", E.cy + 1, E.cx);

  if (len > E.screenCols)
    len = E.screenCols;
  string_buffer_append(sb, status, len);
  while (len < E.screenCols) {
    if (E.screenCols - len == rlen) {
      string_buffer_append(sb, rstatus, rlen);
      break;
    } else {
      string_buffer_append(sb, " ", 1);
      len++;
    }
  }

  string_buffer_append(sb, "\x1b[m", 3);

  string_buffer_append(sb, "\r\n", 3);
}

void editorDrawStatusMessageBar(struct string_buffer *sb) {
  string_buffer_append(sb, "\x1b[K", 3);
  int msglen = strlen(E.status_message);
  if (msglen > E.screenCols)
    msglen = E.screenCols;
  if (msglen && time(NULL) - E.status_message_time < 5) {
    string_buffer_append(sb, E.status_message, msglen);
  }
}
void editorRefreshScreen() {
  editorScroll();
  struct string_buffer sb = STRING_BUF_INIT;

  string_buffer_append(&sb, "\x1b[?25l", 6);
  string_buffer_append(&sb, "\x1b[H", 3);

  editorDrawRows(&sb);
  editorDrawStatusBar(&sb);
  editorDrawStatusMessageBar(&sb);
  char buf[32];

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoffset) + 1,
           (E.rx - E.coloffset) + 1);
  string_buffer_append(&sb, buf, strlen(buf));

  string_buffer_append(&sb, "\x1b[?25h", 6);
  write(STDOUT_FILENO, sb.buffer, sb.len);
  string_buffer_free(&sb);
}
void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.status_message, sizeof(E.status_message), fmt, ap);
  va_end(ap);
  E.status_message_time = time(NULL);
}

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}
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

void editorInsertNewLine() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else if (E.cx == E.row[E.cy].size + E.numDigits) {
    editorInsertRow(E.cy + 1, "", 0);
  } else {
    EditorRow *row = &E.row[E.cy];
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

void editorDelChar() {
  if (E.cy == E.numrows)
    return;
  if (E.cx == E.numDigits && E.cy == 0)
    return;
  EditorRow *row = &E.row[E.cy];
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

void editorMoveCursor(int key) {
  EditorRow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

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

void processNormalModeKeyMap(int key) {
  EditorRow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case CTRL_KEY('b'):
    E.cx = 0;
    break;
  case CTRL_KEY('e'):
    if (row) {
      E.cx = row->size + E.numDigits;
    }
    break;
  case CTRL_KEY('l'):
    selectCurrentRow();
    break;
  case 71:
    E.cy = E.numrows - 1;
    break;
  // h ascii value 104
  case ARROW_LEFT:
  case 104:
    editorMoveCursor(ARROW_LEFT);
    break;
    // j ascii value 106
  case ARROW_DOWN:
  case 106:
    editorMoveCursor(ARROW_DOWN);
    break;
    // k ascii value 107
  case ARROW_UP:
  case 107:
    editorMoveCursor(ARROW_UP);
    break;
    // l ascii value 108
  case ARROW_RIGHT:
  case 108:
    editorMoveCursor(ARROW_RIGHT);
    break;
  case '\x1b':
    E.current_mode = MODE_INSERT;
    break;
  default:
    return;
  }
  return;
}

void processInsertModeKeyMap(int c) {
  static int quit_time = VCODE_QUIT_TIMES;
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
    E.current_mode = MODE_NORMAL;
    break;
  default:
    editorInsertChar(c);
    break;
  }
  quit_time = VCODE_QUIT_TIMES;
}
void editorProcessKeyMap() {
  int c = editorReadKey();
  switch (E.current_mode) {
  case MODE_NORMAL:
    processNormalModeKeyMap(c);
    break;
  case MODE_INSERT:
    processInsertModeKeyMap(c);
    break;
  }
}
