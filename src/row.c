#include "row.h"
#include "editor.h"
#include "highlight.h"
#include <stdlib.h>
#include <string.h>

extern Editor E;

int editorRowCxtoRx(EditorRow *row, int cx) {
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
int editorRowRxToCx(EditorRow *row, int rx) {
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

void ediorUpdateRow(EditorRow *row) {
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
void editorRowAppendString(EditorRow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  ediorUpdateRow(row);
  E.dirty++;
}

void editorInsertRow(int at, char *s, size_t len) {

  if (at < 0 || at > E.numrows)
    return;

  E.row = realloc(E.row, sizeof(EditorRow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(EditorRow) * (E.numrows - at));
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
  E.row[at].selectCurrent = 0;
  ediorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}
void editorFreeRow(EditorRow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows)
    return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(EditorRow) * (E.numrows - at - 1));
  for (int j = at; j < E.numrows - 1; j++)
    E.row[j].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(EditorRow *row, int at, int c) {
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

void editorRowDelChar(EditorRow *row, int at) {
  at = at - E.numDigits;
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  ediorUpdateRow(row);
  E.dirty++;
}

// Selection
void selectCurrentRow() {
  if (E.cy < 0 || E.cy >= E.numrows)
    return;
  EditorRow *row = &E.row[E.cy];

  if (row == NULL)
    return;

  row->selectCurrent = 1;

  ediorUpdateRow(row);
}
