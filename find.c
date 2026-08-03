#include "micro.h"

static char *g_last_search = NULL;  /* last search query, reused by n/N */
static int g_search_dir = 1;        /* 1 = forward (/), -1 = backward (?) */

void editorFindCallback(char *query , int key){
  static int last_match_row = -1;
  static int direction_row = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl){
    memcpy(E.row[saved_hl_line].hl , saved_hl , E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == 'r' || key == '\x1b'){
    last_match_row = -1;
    direction_row = g_search_dir;
    return;
  } else if (key == ARROW_LEFT || key == ARROW_DOWN){
    direction_row = 1;
  } else if (key == ARROW_RIGHT || key == ARROW_UP){
    direction_row = -1;
  } else {
    last_match_row = -1;
    direction_row = g_search_dir;
  }

  if (last_match_row == -1) direction_row = g_search_dir;
  int current_row = last_match_row;

  int i;
  for (i = 0 ; i < E.numrows ; i++){
    current_row += direction_row;
    if (current_row == -1 ) current_row = E.numrows - 1;
    else if (current_row == E.numrows) current_row = 0;

    erow *row = &E.row[current_row];
    char *match = strstr(row->render , query);

    if (match) {
      last_match_row = current_row;
      E.cy = current_row;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;

      saved_hl_line = current_row;

      saved_hl = malloc(row->rsize);
      memcpy(saved_hl , row->hl , row->rsize);

      memset(&row->hl[match - row->render] , HL_MATCH , strlen(query));
      break;
    }
  }
}

void editorFind(int forward){
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  g_search_dir = forward ? 1 : -1;

  char *query = editorPrompt((forward > 0) ? "Search: %s (Use ESC/Arrows/Enter)" : "Search backward: %s (Use ESC/Arrows/Enter)" , editorFindCallback);

  if (query){
    free(g_last_search);
    g_last_search = strdup(query);
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

void editorSearchForward(void) {
  editorFind(1);
}

void editorSearchBackward(void) {
  editorFind(-1);
}

void editorNextMatch(void) {
  if (!g_last_search) { editorSetStatusMessage("No previous search"); return; }
  editorFindCallback(g_last_search, g_search_dir > 0 ? ARROW_DOWN : ARROW_UP);
}

void editorPrevMatch(void) {
  if (!g_last_search) { editorSetStatusMessage("No previous search"); return; }
  editorFindCallback(g_last_search, g_search_dir > 0 ? ARROW_UP : ARROW_DOWN);
}
