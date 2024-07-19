#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

#include "row.h"
#include "string_buffer.h"
#include <termios.h>
#include <time.h>
#define VCODE_VERSION "0.0.1"
#define VCODE_TAB_STOP 4
#define VCODE_QUIT_TIMES 3
#define ANSI_GRAY "\x1b[90m"
#define ANSI_RESET "\x1b[0m"
#define ANSI_WHITE "\x1b[97m"

typedef enum { MODE_INSERT = 1, MODE_NORMAL = 0 } EditorMode;

typedef struct editorConfig {
  int cx, cy;
  int rx;
  int screenRows;
  int screenCols;
  int numrows;
  int numDigits;
  EditorRow *row;
  int dirty;
  int rowoffset;
  int coloffset;
  char *filename;
  int relative_line_no;
  char status_message[80];
  time_t status_message_time;
  EditorMode current_mode;
  struct editorSyntax *syntax;
  struct termios original_termios;
} Editor;

typedef struct editorGitConfig {
  char *currentGitBranch;
} EditorGitConfig;

void initEditor();

void editorOpen(char *filename);

void editorScroll();

void editorDrawRows(struct string_buffer *sb);

void editorDrawStatusBar(struct string_buffer *sb);

void editorDrawStatusMessageBar(struct string_buffer *sb);

void editorSetStatusMessage(const char *fmt, ...);

void editorRefreshScreen();

void die(const char *s);

void editorInsertChar(int c);

void editorInsertNewLine();

void editorDelChar();

void editorProcessKeyMap();

char *editorPromt(char *promt, void (*callback)(char *, int));
#endif
