#ifndef ROW_H
#define ROW_H
#include <stddef.h>

#define SELECT_START "\x1b[48;5;238m" // Background color for selection
#define SELECT_END "\x1b[0m"          // Reset color
typedef struct editorRow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
  int selectCurrent;
} EditorRow;

int editorRowCxtoRx(EditorRow *row, int cx);

int editorRowRxToCx(EditorRow *row, int rx);

void ediorUpdateRow(EditorRow *row);

void editorInsertRow(int at, char *s, size_t len);

void editorFreeRow(EditorRow *row);

void editorDelRow(int at);

void editorRowInsertChar(EditorRow *row, int at, int c);

void editorRowDelChar(EditorRow *row, int at);

void editorRowAppendString(EditorRow *row, char *s, size_t len);

void selectCurrentRow();

void ediorUpdateRow(EditorRow *row);

#endif
