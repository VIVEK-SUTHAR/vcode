#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H
#include "row.h"
struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *single_line_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
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

void editorUpdateSyntax(EditorRow *row);

int editorSyntaxToColor(int hl);

void editorSelectSyntaxHighLight();

#endif // !HIGHLIGHT_H
