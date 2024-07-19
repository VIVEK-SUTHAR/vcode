#include "editor.h"
#include "highlight.h"
#include "row.h"
#include "terminal.h"
extern Editor E;

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
    EditorRow *row = &E.row[current];
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
