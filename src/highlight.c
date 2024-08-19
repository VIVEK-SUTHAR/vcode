#include "highlight.h"
#include "editor.h"
#include "languages.h"
#include "row.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
extern Editor E;

int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}
void editorUpdateSyntax(EditorRow *row) {
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

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
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
