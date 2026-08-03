#include "micro.h"

/*** row operations ***/

int editorRowCxToRx(erow *row , int cx){
  EditorConfig *cfg = config_get();
  int tab_stop = cfg->settings.tab_stop;
  int rx = 0;
  int j;
  for(j = 0 ; j < cx ; j++){
    if (row->chars[j] == '\t')
        rx += (tab_stop - 1) - (rx % tab_stop);
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row , int rx){
  EditorConfig *cfg = config_get();
  int tab_stop = cfg->settings.tab_stop;
  int cur_rx = 0;
  int cx;

  for (cx = 0 ; cx < row->size ; cx++){
    if (row->chars[cx] == '\t')
      cur_rx += (tab_stop - 1) - (cur_rx % tab_stop);
    cur_rx ++;

    if (cur_rx > rx) return cx;

  }
  return cx;
}

void editorUpdateRow(erow *row){
  EditorConfig *cfg = config_get();
  int tab_stop = cfg->settings.tab_stop;
  int tabs = 0;
  int j = 0;
  for (j = 0 ; j < row->size ; j++)
    if (row->chars[j] == '\t') tabs ++;

  free(row->render);
  row->render = malloc(row->size + tabs*(tab_stop - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size ; j++){
    if (row->chars[j] == '\t'){
      row->render[idx++] = ' ';
      while (idx % tab_stop != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  row->rsize = idx;
  editorUpdateSyntax(row);
}

void editorInsertRow(int at , char *s , size_t len){
  if (at  < 0 || at > E.numrows) return;
  E.row = realloc(E.row , sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at+1] , &E.row[at] , sizeof(erow) * (E.numrows - at));

  for (int j = at + 1 ; j <= E.numrows ; j++) E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars , s , len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty ++;
}

void editorFreeRow(erow *row){
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorFreeRows(void){
  for (int j = 0; j < E.numrows; j++) editorFreeRow(&E.row[j]);
  free(E.row);
  E.row = NULL;
  E.numrows = 0;
}

void editorDelRow(int at){
  if (at < 0 || at >= E.numrows) return;

  editorFreeRow(&E.row[at]);
  memmove(&E.row[at],&E.row[at + 1], sizeof(erow)* (E.numrows - at - 1));

  for(int j = at; j < E.numrows - 1 ; j++) E.row[j].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInserChar(erow *row , int at , int c){
  if (at < 0 || at > row-> size) at = row->size;
  row->chars = realloc(row->chars , row->size + 2);
  memmove(&row->chars[at + 1] , &row->chars[at] , row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row , char *s , size_t len){
  row->chars = realloc(row->chars , row->size  + len  + 1);
  memcpy(&row->chars[row->size] , s , len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty ++;
}

void editorRowDelChar(erow *row, int at){
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at+1], row->size - at);
  row->size --;
  editorUpdateRow(row);
  E.dirty ++;
}

/*** editor operations ***/

void editorInsertChar(int c){
  if (E.cy == E.numrows){
    editorInsertRow(E.numrows , "", 0);
  }
  editorRowInserChar(&E.row[E.cy] , E.cx , c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;

  EditorConfig *cfg = config_get();
  if (cfg->settings.auto_indent && E.cy > 0 && E.cy < E.numrows) {
    erow *prev = &E.row[E.cy - 1];
    int ws = 0;
    while (ws < prev->size && (prev->chars[ws] == ' ' || prev->chars[ws] == '\t')) ws++;
    if (ws > 0) {
      erow *newrow = &E.row[E.cy];
      newrow->chars = realloc(newrow->chars, newrow->size + ws + 1);
      memmove(newrow->chars + ws, newrow->chars, newrow->size + 1);
      memcpy(newrow->chars, prev->chars, ws);
      newrow->size += ws;
      editorUpdateRow(newrow);
      E.cx = ws;
    }
  }
}

void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;

  erow *row = &E.row[E.cy];
  if (E.cx > 0){
    editorRowDelChar(row, E.cx - 1);
    E.cx --;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1] , row->chars , row->size);
    editorDelRow(E.cy);
    E.cy --;
  }
}

char* editorRowsToString(int *buflen){
  int totlen = 0;
  int j;

  for (j = 0 ; j < E.numrows ; j++){
    totlen += E.row[j].size + 1;
  }
  *buflen = totlen;

  char *buff = malloc(totlen);
  char *p = buff;

  for (j = 0 ; j< E.numrows ; j++){
    memcpy(p, E.row[j].chars , E.row[j].size );
    p += E.row[j].size; 
    *p = '\n';
    p++;
  }

  return buff;
}

/*** clipboard & line ops ***/

void editorCopyLine(void) {
  if (E.cy >= E.numrows) return;
  erow *row = &E.row[E.cy];
  free(E.clipboard);
  E.clipboard = malloc(row->size + 1);
  memcpy(E.clipboard, row->chars, row->size);
  E.clipboard[row->size] = '\0';
  E.clipboard_len = row->size;
  editorSetStatusMessage("Line copied");
}

void editorPasteAfter(void) {
  if (!E.clipboard) return;
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorMoveCursor(ARROW_RIGHT);
  editorRowAppendString(&E.row[E.cy], E.clipboard, E.clipboard_len);
  editorSetStatusMessage("Pasted after");
}

void editorPasteBefore(void) {
  if (!E.clipboard) return;
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInserChar(&E.row[E.cy], E.cx, '\0');
  editorRowAppendString(&E.row[E.cy], E.clipboard, E.clipboard_len);
  editorSetStatusMessage("Pasted before");
}

void editorDeleteLine(void) {
  if (E.cy >= E.numrows) return;
  editorDelRow(E.cy);
  if (E.cy >= E.numrows) E.cy = E.numrows - 1;
  E.cx = 0;
  editorSetStatusMessage("Line deleted");
}
