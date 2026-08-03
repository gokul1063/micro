#include "micro.h"

void editorSaveTabState(void) {
  Tab *t = &tabs[cur_tab];
  t->row = E.row;
  t->numrows = E.numrows;
  t->cx = E.cx;
  t->cy = E.cy;
  t->rx = E.rx;
  t->rowoff = E.rowoff;
  t->coloff = E.coloff;
  t->dirty = E.dirty;
  t->filename = E.filename;
  t->recfile = E.recfile;
  t->syntax = E.syntax;
  t->mode = E.mode;
  t->visual_mode = E.visual_mode;
  t->visual_cy = E.visual_cy;
  t->visual_cx = E.visual_cx;

  /* transfer ownership of the buffer to the tab */
  E.row = NULL;
  E.numrows = 0;
  E.filename = NULL;
  E.recfile = NULL;
  E.syntax = NULL;
  E.dirty = 0;
}

void editorLoadTabState(void) {
  Tab *t = &tabs[cur_tab];
  E.row = t->row;
  E.numrows = t->numrows;
  E.cx = t->cx;
  E.cy = t->cy;
  E.rx = t->rx;
  E.rowoff = t->rowoff;
  E.coloff = t->coloff;
  E.dirty = t->dirty;
  E.filename = t->filename;
  E.recfile = t->recfile;
  E.syntax = t->syntax;
  E.mode = t->mode;
  E.visual_mode = t->visual_mode;
  E.visual_cy = t->visual_cy;
  E.visual_cx = t->visual_cx;

  /* the buffer is now owned by the active editor */
  t->row = NULL;
  t->numrows = 0;
  t->filename = NULL;
  t->recfile = NULL;
  t->syntax = NULL;
  t->dirty = 0;

  if (E.cy < 0) E.cy = 0;
  if (E.cy > E.numrows) E.cy = E.numrows;
  if (E.cy < E.numrows && E.cx > E.row[E.cy].size) E.cx = E.row[E.cy].size;

  editorSetStatusMessage("Tab %d/%d", cur_tab + 1, tab_count);
}

void editorNewTab(void) {
  if (tab_count >= MICRO_MAX_TABS) {
    editorSetStatusMessage("Maximum %d tabs reached", MICRO_MAX_TABS);
    return;
  }

  if (E.dirty) editorWriteRecFile();
  editorSaveTabState();

  cur_tab = tab_count;
  tab_count++;

  Tab *t = &tabs[cur_tab];
  memset(t, 0, sizeof(*t));

  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.mode = MODE_NORMAL;
  E.visual_mode = 0;
  E.visual_cy = 0;
  E.visual_cx = 0;

  editorSetStatusMessage("New tab %d/%d", cur_tab + 1, tab_count);
}

void editorNextTab(void) {
  if (tab_count <= 1) return;
  if (E.dirty) editorWriteRecFile();
  editorSaveTabState();
  cur_tab = (cur_tab + 1) % tab_count;
  editorLoadTabState();
}

void editorPrevTab(void) {
  if (tab_count <= 1) return;
  if (E.dirty) editorWriteRecFile();
  editorSaveTabState();
  cur_tab = (cur_tab - 1 + tab_count) % tab_count;
  editorLoadTabState();
}

static void editorCloseTabImpl(int force) {
  if (!force && E.dirty) {
    char *answer = editorPrompt("Unsaved changes. Save before closing this tab? (y/n) %s", NULL);
    if (answer == NULL) {
      editorSetStatusMessage("Tab close cancelled");
      return;
    }
    char c = answer[0];
    free(answer);

    if (c == 'y' || c == 'Y') {
      editorSave();
      if (E.dirty) {
        editorSetStatusMessage("Tab close cancelled");
        return;
      }
    } else if (c == 'n' || c == 'N') {
      /* discard changes */
    } else {
      editorSetStatusMessage("Tab close cancelled");
      return;
    }
  }

  if (tab_count == 1) {
    /* last tab: closing it quits the editor */
    editorQuit();
    return;
  }

  /* free the focused tab's resources (buffer is owned by the active editor) */
  editorFreeRows();
  free(E.filename);
  E.filename = NULL;
  if (E.recfile) {
    remove(E.recfile);
    free(E.recfile);
    E.recfile = NULL;
  }

  /* remove the tab slot and compact the array */
  for (int i = cur_tab; i < tab_count - 1; i++) {
    tabs[i] = tabs[i + 1];
  }
  tab_count--;

  if (cur_tab >= tab_count) cur_tab = tab_count - 1;

  editorLoadTabState();
  editorSetStatusMessage("Tab closed (%d left)", tab_count);
}

void editorCloseTab(void) {
  editorCloseTabImpl(0);
}

void editorForceCloseTab(void) {
  editorCloseTabImpl(1);
}
