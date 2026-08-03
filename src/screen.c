#include "micro.h"

/*** append buffer ***/

void abAppend(struct abuf *ab , const char *s , int len){
  char *new = realloc(ab->buffer , ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len] , s , len);
  ab->buffer = new;
  ab->len += len;
}

void abFree(struct abuf *ab){
  free(ab->buffer);
}

void abAppendFg(struct abuf *ab , RGB c){
  char buf[32];
  int len = snprintf(buf , sizeof(buf) , "\x1b[38;2;%d;%d;%dm" , c.r , c.g , c.b);
  abAppend(ab , buf , len);
}

/*** status message ***/

void editorSetStatusMessage(const char *fmt , ...){
  va_list ap;
  va_start(ap , fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg) , fmt , ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** output ***/

void editorScroll(){
  E.rx = 0;

  if (E.cy < E.numrows){
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff){
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows){
    E.rowoff = E.cy - E.screenrows + 1;
  }

  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols){
    E.coloff = E.rx - E.screencols;
  }
}

int editorLineNumWidth(void) {
  EditorConfig *cfg = config_get();
  if (!cfg->settings.show_line_numbers) return 0;
  int n = E.numrows > 0 ? E.numrows : 1;
  int digits = 1;
  while (n >= 10) { n /= 10; digits++; }
  return digits + 1; /* digits + trailing space */
}

void editorDrawRows(struct abuf *ab){
  EditorConfig *cfg = config_get();
  int y;
  // Visual selection bounds
  int sel_start_y = -1, sel_start_x = -1, sel_end_y = -1, sel_end_x = -1;
  int linewise = 0;
  if (E.visual_mode) {
    linewise = (E.visual_mode == 2);
    sel_start_y = E.visual_cy;
    sel_start_x = E.visual_cx;
    sel_end_y = E.cy;
    sel_end_x = E.cx;
    if (sel_start_y > sel_end_y || (sel_start_y == sel_end_y && sel_start_x > sel_end_x)) {
      int tmp_y = sel_start_y; sel_start_y = sel_end_y; sel_end_y = tmp_y;
      int tmp_x = sel_start_x; sel_start_x = sel_end_x; sel_end_x = tmp_x;
    }
  }
  for (y = 0 ; y < E.screenrows ; y++){
    int filerow = y + E.rowoff;

    // Line numbers
    if (cfg->settings.show_line_numbers) {
      int lnw = editorLineNumWidth();
      char linenum[16];
      int linenum_len = 0;
      if (filerow < E.numrows) {
        linenum_len = snprintf(linenum, sizeof(linenum), "%*d ", lnw - 1, filerow + 1);
      } else {
        linenum_len = snprintf(linenum, sizeof(linenum), "%*s", lnw, "");
      }

      if (cfg->settings.highlight_current_line && filerow == E.cy) {
        char buf[64];
        int clen = snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm\x1b[48;2;30;30;30m",
                           cfg->colors.match.r, cfg->colors.match.g, cfg->colors.match.b);
        abAppend(ab, buf, clen);
      } else {
        char buf[64];
        int clen = snprintf(buf, sizeof(buf), "\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm",
                           cfg->colors.line_numbers_fg.r, cfg->colors.line_numbers_fg.g, cfg->colors.line_numbers_fg.b,
                           cfg->colors.line_numbers_bg.r, cfg->colors.line_numbers_bg.g, cfg->colors.line_numbers_bg.b);
        abAppend(ab, buf, clen);
      }
      abAppend(ab, linenum, linenum_len);
      abAppend(ab, "\x1b[m", 3);
    }

    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows /3 ){
        char welcome[80];
        int welcomelen = snprintf(welcome , sizeof(welcome),
            "miro editor -- version %s" , MICRO_VERSION);
        if (welcomelen > E.screencols) welcomelen  = E.screencols;
        int padding = (E.screencols - welcomelen ) / 2;
        if (padding){
          abAppend(ab , "~" , 1);
          padding -- ;
        }
        while (padding--) abAppend(ab , " " , 1);
        abAppend(ab , welcome , welcomelen);

      } else if (E.numrows == 0 && y == E.screenrows /3 + 1){
        char hint[] = "Tab = new tab | Alt+Tab = prev tab | :e <file> = open file";
        int hintlen = strlen(hint);
        if (hintlen > E.screencols) hintlen  = E.screencols;
        int padding = (E.screencols - hintlen ) / 2;
        if (padding){
          abAppend(ab , "~" , 1);
          padding -- ;
        }
        while (padding--) abAppend(ab , " " , 1);
        abAppend(ab , hint , hintlen);

      } else {
        abAppend(ab , "~",1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;

      // Determine if this line is selected
      int line_selected = 0;
      if (E.visual_mode) {
        if (linewise) {
          if (filerow >= sel_start_y && filerow <= sel_end_y) line_selected = 1;
        } else {
          if (sel_start_y == sel_end_y) {
            if (filerow == sel_start_y) line_selected = 1;
          } else {
            if (filerow == sel_start_y || filerow == sel_end_y || (filerow > sel_start_y && filerow < sel_end_y))
              line_selected = 1;
          }
        }
      }

      // Highlight current line
      if (cfg->settings.highlight_current_line && filerow == E.cy) {
        abAppend(ab, "\x1b[7m", 4);
      }
      if (line_selected) {
        abAppend(ab, "\x1b[7m", 4);
      }

      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int currnet_hl = -1;

      int j;
      for (j = 0 ; j < len ; j++){
        if (iscntrl(c[j])){
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab , "\x1b[7m" , 4);
          abAppend(ab , &sym , 1);
          abAppend(ab , "\x1b[m" , 3);
          if (currnet_hl != -1){
            abAppendFg(ab , editorSyntaxToColor(currnet_hl));
          } else {
            abAppendFg(ab , cfg->colors.foreground);
          }

        } else if (hl[j] == HL_NORMAL){

          if (currnet_hl != -1){
            abAppend(ab,"\x1b[39m" , 5);
            currnet_hl = -1;
          }
          abAppend(ab , &c[j] , 1);

        } else {
          if (hl[j] != currnet_hl){

            currnet_hl = hl[j];
            abAppendFg(ab , editorSyntaxToColor(hl[j]));
          }

          abAppend(ab , &c[j] , 1);
        }
      }

      abAppend(ab , "\x1b[39m" , 5);
      abAppend(ab , "\x1b[m" , 3);
    }
    abAppend(ab , "\x1b[K" , 3);
    abAppend(ab,"\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab){
  EditorConfig *cfg = config_get();
  char mode_str[16];
  switch (E.mode) {
    case MODE_NORMAL: strcpy(mode_str, "NORMAL"); break;
    case MODE_INSERT: strcpy(mode_str, "INSERT"); break;
    case MODE_VISUAL: strcpy(mode_str, "VISUAL"); break;
  }

  char status_fg[32], status_bg[32];
  snprintf(status_fg, sizeof(status_fg), "\x1b[38;2;%d;%d;%dm",
           cfg->colors.status_bar_fg.r, cfg->colors.status_bar_fg.g, cfg->colors.status_bar_fg.b);
  snprintf(status_bg, sizeof(status_bg), "\x1b[48;2;%d;%d;%dm",
           cfg->colors.status_bar_bg.r, cfg->colors.status_bar_bg.g, cfg->colors.status_bar_bg.b);

  abAppend(ab, status_fg, strlen(status_fg));
  abAppend(ab, status_bg, strlen(status_bg));

  char status[80] , rstatus[80];

  int len = snprintf(status , sizeof(status) , " %s %.20s - %d lines %s" , mode_str, E.filename ? E.filename : "[No Name]" , E.numrows , E.dirty ? "(modified)" : "");

  int rlen = snprintf(rstatus , sizeof(rstatus) , "%s | %d-%d | T%d/%d " ,(E.syntax ) ? E.syntax->filetype : "no ft", E.cy + 1 , E.numrows, cur_tab + 1, tab_count);

  if (len > E.screencols) len = E.screencols;
  abAppend(ab , status , len);

  while (len < E.screencols){
    if (E.screencols - len == rlen){
      abAppend(ab,rstatus , rlen);
      break;
    } else {
      abAppend(ab , " " , 1);
      len++;
    }
  }

  abAppend(ab , "\x1b[m" , 3);
  abAppend(ab , "\r\n" , 2);
}

void editorDrawMessageBar(struct abuf *ab){
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab , E.statusmsg , msglen);
}

void editorRefreshScreen(){
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab , "\x1b[?25l" , 6); 
  abAppend(&ab , "\x1b[H" , 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  int gutter = editorLineNumWidth();

  char buf[32];
  snprintf(buf , sizeof(buf) , "\x1b[%d;%dH" , (E.cy - E.rowoff) + 1,
                                               (E.rx - E.coloff) + 1 + gutter);
  abAppend(&ab , buf , strlen(buf));

  /* solid block cursor (default style) */
  abAppend(&ab , "\x1b[1 q" , 5);

  /* show the cursor and enable blinking */
  abAppend(&ab , "\x1b[?25h\x1b[?12h" , 12);
  write(STDOUT_FILENO , ab.buffer , ab.len);
  abFree(&ab);
}
