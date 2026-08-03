#include "micro.h"

/*** input ***/

char* editorPrompt(char* prompt , void(*callback)(char *, int)){
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';

  while(1){
    editorSetStatusMessage(prompt , buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE){
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b'){
      editorSetStatusMessage("");
      if (callback) callback(buf,c);
      free(buf);
      return NULL;
    } else if (c == '\r'){
      if (buflen != 0){
        editorSetStatusMessage("");
        if (callback) callback(buf , c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128){
      if (buflen == bufsize - 1){
        bufsize *= 2;
        buf = realloc(buf , bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }

    if (callback) callback(buf,c);
  }
}

void editorMoveCursor(int key){
  erow *row = (E.cy >= E.numrows ) ? NULL : &E.row[E.cy];
  switch (key){
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy --;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if(row && E.cx < row->size){
        E.cx++;
      } else if (row && E.cx == row->size){
        E.cy ++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen){
    E.cx = rowlen;
  }
}

static int is_word_char(int c) {
  return isalnum(c) || c == '_';
}

/* w: jump to start of next word */
void editorMoveWord(void) {
  while (E.cy < E.numrows) {
    erow *row = &E.row[E.cy];
    int j = E.cx;
    if (j < row->size) {
      if (is_word_char(row->chars[j])) {
        while (j < row->size && is_word_char(row->chars[j])) j++;
      }
      while (j < row->size && !is_word_char(row->chars[j])) j++;
      if (j < row->size) { E.cx = j; return; }
    }
    E.cy++;
    E.cx = 0;
  }
}

/* e: jump to end of current or next word */
void editorMoveWordEnd(void) {
  while (E.cy < E.numrows) {
    erow *row = &E.row[E.cy];
    int j = E.cx;
    if (j < row->size) {
      if (is_word_char(row->chars[j])) {
        while (j < row->size && is_word_char(row->chars[j])) j++;
        E.cx = (j > 0) ? j - 1 : 0;
        return;
      } else {
        while (j < row->size && !is_word_char(row->chars[j])) j++;
        if (j < row->size) { E.cx = j; continue; }
      }
    }
    E.cy++;
    E.cx = 0;
  }
}

/* b: jump to start of previous word */
void editorMoveWordBack(void) {
  while (E.cy >= 0) {
    erow *row = &E.row[E.cy];
    int j = E.cx;
    if (j > 0 && is_word_char(row->chars[j - 1])) {
      while (j > 0 && is_word_char(row->chars[j - 1])) j--;
      E.cx = j;
      return;
    }
    while (j > 0 && !is_word_char(row->chars[j - 1])) j--;
    if (j > 0) {
      E.cx = j;
      while (E.cx > 0 && is_word_char(row->chars[E.cx - 1])) E.cx--;
      return;
    }
    if (E.cy == 0) return;
    E.cy--;
    E.cx = E.row[E.cy].size;
  }
}

/*** modes ***/

void editorSetMode(EditorMode mode) {
  E.mode = mode;
}

void editorInsertMode(void) {
  E.mode = MODE_INSERT;
  editorBeginUndoGroup();
  editorSetStatusMessage("-- INSERT --");
}

void editorNormalMode(void) {
  E.mode = MODE_NORMAL;
  editorEndUndoGroup();
  editorSetStatusMessage("");
}

void editorEnterVisualMode(int linewise) {
  E.visual_mode = linewise ? 2 : 1; // 1 charwise, 2 linewise
  E.visual_cy = E.cy;
  E.visual_cx = E.cx;
  if (linewise) {
    E.visual_cx = 0;
  }
  editorSetStatusMessage(linewise ? "-- VISUAL LINE --" : "-- VISUAL --");
}

void editorExitVisualMode(void) {
  E.visual_mode = 0;
  editorNormalMode();
}

void editorYankSelection(void) {
  if (!E.visual_mode) return;
  int start_y = E.visual_cy;
  int start_x = E.visual_cx;
  int end_y = E.cy;
  int end_x = E.cx;
  // Ensure start <= end
  if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
    int tmp_y = start_y; start_y = end_y; end_y = tmp_y;
    int tmp_x = start_x; start_x = end_x; end_x = tmp_x;
  }
  // Calculate total length
  int total_len = 0;
  if (start_y == end_y) {
    total_len = end_x - start_x;
  } else {
    // first line
    total_len += E.row[start_y].size - start_x;
    // middle lines
    for (int y = start_y + 1; y < end_y; y++) {
      total_len += E.row[y].size + 1; // +1 for newline
    }
    // last line
    total_len += end_x;
  }
  char *buf = malloc(total_len + 1);
  int pos = 0;
  if (start_y == end_y) {
    memcpy(buf + pos, E.row[start_y].chars + start_x, total_len);
    pos += total_len;
  } else {
    // first line
    int len = E.row[start_y].size - start_x;
    memcpy(buf + pos, E.row[start_y].chars + start_x, len);
    pos += len;
    buf[pos++] = '\n';
    // middle lines
    for (int y = start_y + 1; y < end_y; y++) {
      memcpy(buf + pos, E.row[y].chars, E.row[y].size);
      pos += E.row[y].size;
      buf[pos++] = '\n';
    }
    // last line
    memcpy(buf + pos, E.row[end_y].chars, end_x);
    pos += end_x;
  }
  buf[pos] = '\0';
  free(E.clipboard);
  E.clipboard = buf;
  E.clipboard_len = pos;
  editorSetStatusMessage("Selection yanked");
}

void editorDeleteSelection(void) {
  if (!E.visual_mode) return;
  int start_y = E.visual_cy;
  int start_x = E.visual_cx;
  int end_y = E.cy;
  int end_x = E.cx;
  // Ensure start <= end
  if (start_y > end_y || (start_y == end_y && start_x > end_x)) {
    int tmp_y = start_y; start_y = end_y; end_y = tmp_y;
    int tmp_x = start_x; start_x = end_x; end_x = tmp_x;
  }
  if (E.visual_mode == 2) {
    // linewise: delete whole lines start_y..end_y
    for (int i = 0; i <= end_y - start_y; i++) {
      editorDelRow(start_y);
    }
    if (start_y >= E.numrows) start_y = E.numrows - 1;
    if (start_y < 0) start_y = 0;
    E.cy = start_y;
    E.cx = 0;
  } else {
    // charwise
    if (start_y == end_y) {
      erow *row = &E.row[start_y];
      int del_len = end_x - start_x;
      memmove(row->chars + start_x, row->chars + end_x, row->size - end_x + 1);
      row->size -= del_len;
      editorUpdateRow(row);
      E.cy = start_y;
      E.cx = start_x;
    } else {
      // first line: keep up to start_x
      erow *first = &E.row[start_y];
      int first_keep = start_x;
      // last line: keep from end_x to end
      erow *last = &E.row[end_y];
      char *last_tail = last->chars + end_x;
      int last_tail_len = last->size - end_x;
      // delete middle lines entirely
      for (int y = start_y + 1; y <= end_y; y++) {
        editorDelRow(start_y + 1);
      }
      // Append last_tail to first line
      first->chars = realloc(first->chars, first_keep + last_tail_len + 1);
      memcpy(first->chars + first_keep, last_tail, last_tail_len);
      first->size = first_keep + last_tail_len;
      first->chars[first->size] = '\0';
      editorUpdateRow(first);
      E.cy = start_y;
      E.cx = first_keep;
    }
  }
  editorSetStatusMessage("Selection deleted");
}

void editorVisualModeProcessKey(int c) {
  EditorConfig *cfg = config_get();
  int up_key = config_key_to_code(cfg->normal_keys.normal_move_up);
  int down_key = config_key_to_code(cfg->normal_keys.normal_move_down);
  int left_key = config_key_to_code(cfg->normal_keys.normal_move_left);
  int right_key = config_key_to_code(cfg->normal_keys.normal_move_right);
  int home_key = config_key_to_code(cfg->normal_keys.normal_move_line_start);
  int end_key = config_key_to_code(cfg->normal_keys.normal_move_line_end);
  int page_up_key = config_key_to_code(cfg->normal_keys.normal_page_up);
  int page_down_key = config_key_to_code(cfg->normal_keys.normal_page_down);
  int copy_line_key = config_key_to_code(cfg->normal_keys.normal_copy_line);
  int delete_line_key = config_key_to_code(cfg->normal_keys.normal_delete_line);

  // Movement keys extend selection
  if (c == up_key || c == ARROW_UP) {
    editorMoveCursor(ARROW_UP);
  } else if (c == down_key || c == ARROW_DOWN) {
    editorMoveCursor(ARROW_DOWN);
  } else if (c == left_key || c == ARROW_LEFT) {
    editorMoveCursor(ARROW_LEFT);
  } else if (c == right_key || c == ARROW_RIGHT) {
    editorMoveCursor(ARROW_RIGHT);
  } else if (c == home_key) {
    E.cx = 0;
  } else if (c == end_key) {
    if (E.cy < E.numrows) E.cx = E.row[E.cy].size;
  } else if (c == page_up_key) {
    for (int i = 0; i < E.screenrows / 2; i++) editorMoveCursor(ARROW_UP);
  } else if (c == page_down_key) {
    for (int i = 0; i < E.screenrows / 2; i++) editorMoveCursor(ARROW_DOWN);
  } else if (c == 'y' || c == copy_line_key) {
    // yank selection
    editorYankSelection();
    editorExitVisualMode();
  } else if (c == 'd' || c == delete_line_key) {
    // delete selection
    editorUndoPush();
    editorDeleteSelection();
    editorExitVisualMode();
  } else if (c == '\x1b' || c == 'v' || c == 'V') {
    editorExitVisualMode();
  }
  // else ignore other keys
}

void editorGotoLine(void) {
  char *cmd = editorPrompt(":%s", NULL);
  if (cmd == NULL) return;

  char *p = cmd;
  while (*p == ' ') p++;

  if (strcmp(p, "w") == 0 || strcmp(p, "w!") == 0) {
    editorSave();
  } else if (strcmp(p, "q") == 0) {
    editorCloseTab();
  } else if (strcmp(p, "q!") == 0) {
    editorForceCloseTab();
  } else if (strcmp(p, "wq") == 0) {
    editorSave();
    if (E.dirty == 0) editorCloseTab();
  } else if (strcmp(p, "tabnew") == 0) {
    editorNewTab();
  } else if (*p == 'e') {
    p++;
    while (*p == ' ') p++;
    if (*p != '\0') {
      char *file = p;
      int created = (access(file, F_OK) != 0);
      if (E.dirty) editorWriteRecFile();
      editorOpen(file);
      editorSetStatusMessage(created ? "Created %s" : "Opened %s", file);
    } else {
      editorSetStatusMessage("Usage: :e <filename>");
    }
  } else {
    int line = atoi(p);
    if (line > 0 && line <= E.numrows) {
      E.cy = line - 1;
      E.cx = 0;
      E.rowoff = E.cy;
    }
  }
  free(cmd);
}

/*** key processor ***/

void editorProcessKey(){
  EditorConfig *cfg = config_get();
  int c = editorReadKey();

  if (c == -1) return;  /* interrupted by a signal (resize); main loop will redraw */

  if (E.mode == MODE_NORMAL) {
    int quit_key = config_key_to_code(cfg->normal_keys.normal_quit);
    int save_key = config_key_to_code(cfg->normal_keys.normal_save);
    int find_key = config_key_to_code(cfg->normal_keys.normal_find);
    int up_key = config_key_to_code(cfg->normal_keys.normal_move_up);
    int down_key = config_key_to_code(cfg->normal_keys.normal_move_down);
    int left_key = config_key_to_code(cfg->normal_keys.normal_move_left);
    int right_key = config_key_to_code(cfg->normal_keys.normal_move_right);
    int home_key = config_key_to_code(cfg->normal_keys.normal_move_line_start);
    int end_key = config_key_to_code(cfg->normal_keys.normal_move_line_end);
    int page_up_key = config_key_to_code(cfg->normal_keys.normal_page_up);
    int page_down_key = config_key_to_code(cfg->normal_keys.normal_page_down);
    int insert_key = config_key_to_code(cfg->normal_keys.normal_enter_insert);
    int insert_after_key = config_key_to_code(cfg->normal_keys.normal_enter_insert_after);
    int insert_line_start_key = config_key_to_code(cfg->normal_keys.normal_enter_insert_line_start);
    int insert_line_end_key = config_key_to_code(cfg->normal_keys.normal_enter_insert_line_end);
    int insert_below_key = config_key_to_code(cfg->normal_keys.normal_enter_insert_new_line_below);
    int insert_above_key = config_key_to_code(cfg->normal_keys.normal_enter_insert_new_line_above);
    int delete_char_key = config_key_to_code(cfg->normal_keys.normal_delete_char);
    int delete_line_key = config_key_to_code(cfg->normal_keys.normal_delete_line);
    int undo_key = config_key_to_code(cfg->normal_keys.normal_undo);
    int redo_key = config_key_to_code(cfg->normal_keys.normal_redo);
    int search_fwd_key = config_key_to_code(cfg->normal_keys.normal_search_forward);
    int search_bwd_key = config_key_to_code(cfg->normal_keys.normal_search_backward);
    int next_match_key = config_key_to_code(cfg->normal_keys.normal_next_match);
    int prev_match_key = config_key_to_code(cfg->normal_keys.normal_prev_match);
    int goto_line_key = config_key_to_code(cfg->normal_keys.normal_goto_line);
    int copy_line_key = config_key_to_code(cfg->normal_keys.normal_copy_line);
    int paste_after_key = config_key_to_code(cfg->normal_keys.normal_paste_after);
    int paste_before_key = config_key_to_code(cfg->normal_keys.normal_paste_before);

    /* two-key commands (dd, yy, gg, gG) */
    static int pending_key = 0;
    if (pending_key) {
      int p = pending_key;
      pending_key = 0;
      if (p == 'd' && c == 'd') { editorUndoPush(); editorDeleteLine(); return; }
      if (p == 'y' && c == 'y') { editorCopyLine(); return; }
      if (p == 'g' && c == 'g') { E.cy = 0; E.cx = 0; E.rowoff = 0; return; }
      if (p == 'g' && c == 'G') { E.cy = E.numrows - 1; if (E.cy >= 0) E.cx = E.row[E.cy].size; return; }
      /* otherwise: discard pending and process c normally below */
    }
    if (c == 'd' || c == 'y' || c == 'g') {
      pending_key = c;
      return;
    }

    switch (c) {
      case '\r':
        editorInsertMode();
        editorInsertNewline();
        break;
      case '^':
        if (E.cy < E.numrows) {
          erow *row = &E.row[E.cy];
          int j = 0;
          while (j < row->size && (row->chars[j] == ' ' || row->chars[j] == '\t')) j++;
          E.cx = j;
        }
        break;
      case 'w':
        editorMoveWord();
        break;
      case 'e':
        editorMoveWordEnd();
        break;
      case 'b':
        editorMoveWordBack();
        break;
      case 'G':
        E.cy = E.numrows - 1;
        if (E.cy >= 0) E.cx = E.row[E.cy].size;
        break;
      case CTRL_KEY('q'):
        editorCloseTab();
        break;
      case CTRL_KEY('s'):
        editorSave();
        break;
      case CTRL_KEY('f'):
        editorFind(1);
        break;
      case CTRL_KEY('h'):
      case DEL_KEY:
      case BACKSPACE:
        editorUndoPush();
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
      case CTRL_KEY('u'):
        for (int i = 0; i < E.screenrows / 2; i++) editorMoveCursor(ARROW_UP);
        break;
      case CTRL_KEY('d'):
        for (int i = 0; i < E.screenrows / 2; i++) editorMoveCursor(ARROW_DOWN);
        break;
      case CTRL_KEY('l'):
      case '\x1b':
        break;
      case 'v':
        editorEnterVisualMode(0);
        break;
      case 'V':
        editorEnterVisualMode(1);
        break;
      case '\t':
        if (cur_tab == tab_count - 1) {
          editorNewTab();   /* at the last tab: open a fresh one */
        } else {
          editorNextTab();  /* otherwise move forward (rotates) */
        }
        break;
      case ALT_TAB:
        editorPrevTab();
        break;
      default:
        if (c == quit_key) {
          editorCloseTab();
        } else if (c == save_key) {
          editorSave();
        } else if (c == find_key) {
          editorFind(1);
        } else if (c == up_key) {
          editorMoveCursor(ARROW_UP);
        } else if (c == down_key) {
          editorMoveCursor(ARROW_DOWN);
        } else if (c == left_key) {
          editorMoveCursor(ARROW_LEFT);
        } else if (c == right_key) {
          editorMoveCursor(ARROW_RIGHT);
        } else if (c == home_key) {
          E.cx = 0;
        } else if (c == end_key) {
          if (E.cy < E.numrows) E.cx = E.row[E.cy].size;
        } else if (c == page_up_key) {
          for (int i = 0; i < E.screenrows / 2; i++) editorMoveCursor(ARROW_UP);
        } else if (c == page_down_key) {
          for (int i = 0; i < E.screenrows / 2; i++) editorMoveCursor(ARROW_DOWN);
        } else if (c == insert_key) {
          editorInsertMode();
        } else if (c == insert_after_key) {
          editorMoveCursor(ARROW_RIGHT);
          editorInsertMode();
        } else if (c == insert_line_start_key) {
          E.cx = 0;
          editorInsertMode();
        } else if (c == insert_line_end_key) {
          if (E.cy < E.numrows) E.cx = E.row[E.cy].size;
          editorInsertMode();
        } else if (c == insert_below_key) {
          editorUndoPush();
          editorInsertNewline();
          editorInsertMode();
        } else if (c == insert_above_key) {
          editorUndoPush();
          if (E.cy > 0) { E.cy--; }
          E.cx = 0;
          editorInsertNewline();
          editorMoveCursor(ARROW_UP);
          editorInsertMode();
        } else if (c == delete_char_key) {
          editorUndoPush();
          editorDelChar();
        } else if (c == delete_line_key) {
          editorUndoPush();
          editorDeleteLine();
        } else if (c == undo_key) {
          editorUndo();
        } else if (c == redo_key) {
          editorRedo();
        } else if (c == search_fwd_key) {
          editorSearchForward();
        } else if (c == search_bwd_key) {
          editorSearchBackward();
        } else if (c == next_match_key) {
          editorNextMatch();
        } else if (c == prev_match_key) {
          editorPrevMatch();
        } else if (c == goto_line_key) {
          editorGotoLine();
        } else if (c == copy_line_key) {
          editorCopyLine();
        } else if (c == paste_after_key) {
          editorUndoPush();
          editorPasteAfter();
        } else if (c == paste_before_key) {
          editorUndoPush();
          editorPasteBefore();
        } else if (c == ARROW_UP) {
          editorMoveCursor(ARROW_UP);
        } else if (c == ARROW_DOWN) {
          editorMoveCursor(ARROW_DOWN);
        } else if (c == ARROW_LEFT) {
          editorMoveCursor(ARROW_LEFT);
        } else if (c == ARROW_RIGHT) {
          editorMoveCursor(ARROW_RIGHT);
        } else if (c == HOME_KEY) {
          E.cx = 0;
        } else if (c == END_KEY) {
          if (E.cy < E.numrows) E.cx = E.row[E.cy].size;
        } else if (c == PAGE_UP) {
          for (int i = 0; i < E.screenrows; i++) editorMoveCursor(ARROW_UP);
        } else if (c == PAGE_DOWN) {
          for (int i = 0; i < E.screenrows; i++) editorMoveCursor(ARROW_DOWN);
        }
        break;
    }
  } else if (E.mode == MODE_INSERT) {
    int exit_key = config_key_to_code(cfg->insert_keys.exit_insert);
    int save_key = config_key_to_code(cfg->insert_keys.save);
    int newline_key = config_key_to_code(cfg->insert_keys.newline);
    int backspace_key = config_key_to_code(cfg->insert_keys.backspace);
    int delete_key = config_key_to_code(cfg->insert_keys.delete_key);
    int up_key = config_key_to_code(cfg->insert_keys.move_up);
    int down_key = config_key_to_code(cfg->insert_keys.move_down);
    int left_key = config_key_to_code(cfg->insert_keys.move_left);
    int right_key = config_key_to_code(cfg->insert_keys.move_right);

    switch (c) {
      case CTRL_KEY('s'):
        editorSave();
        break;
      case '\r':
        editorInsertNewline();
        break;
      case CTRL_KEY('h'):
      case DEL_KEY:
      case BACKSPACE:
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
      case '\x1b':
      case CTRL_KEY('c'):
        editorNormalMode();
        break;
      default:
        if (c == exit_key) {
          editorNormalMode();
        } else if (c == save_key) {
          editorSave();
        } else if (c == newline_key || c == '\r') {
          editorInsertNewline();
        } else if (c == backspace_key || c == BACKSPACE || c == CTRL_KEY('h')) {
          editorDelChar();
        } else if (c == delete_key || c == DEL_KEY) {
          editorMoveCursor(ARROW_RIGHT);
          editorDelChar();
        } else if (c == up_key || c == ARROW_UP) {
          editorMoveCursor(ARROW_UP);
        } else if (c == down_key || c == ARROW_DOWN) {
          editorMoveCursor(ARROW_DOWN);
        } else if (c == left_key || c == ARROW_LEFT) {
          editorMoveCursor(ARROW_LEFT);
        } else if (c == right_key || c == ARROW_RIGHT) {
          editorMoveCursor(ARROW_RIGHT);
        } else if (c == HOME_KEY) {
          E.cx = 0;
        } else if (c == END_KEY) {
          if (E.cy < E.numrows) E.cx = E.row[E.cy].size;
        } else if (c == PAGE_UP) {
          for (int i = 0; i < E.screenrows; i++) editorMoveCursor(ARROW_UP);
        } else if (c == PAGE_DOWN) {
          for (int i = 0; i < E.screenrows; i++) editorMoveCursor(ARROW_DOWN);
        } else if (!iscntrl(c) || c == '\t') {
          editorInsertChar(c);
        }
        break;
    }
  } else if (E.visual_mode) {
    editorVisualModeProcessKey(c);
  }
}
