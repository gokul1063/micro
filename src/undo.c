#include "micro.h"

#define UNDO_MAX 50

typedef struct {
  erow *row;
  int numrows;
  int cx, cy, rx;
  int rowoff, coloff;
  int dirty;
} UndoState;

static UndoState undo_stack[UNDO_MAX];
static int undo_len = 0;
static UndoState redo_stack[UNDO_MAX];
static int redo_len = 0;

static int undo_insert_group = 0;

static erow *clone_rows(const erow *src_rows, int numrows) {
  if (numrows == 0) return NULL;
  erow *rows = malloc(sizeof(erow) * numrows);
  for (int i = 0; i < numrows; i++) {
    const erow *src = &src_rows[i];
    erow *dst = &rows[i];
    dst->idx = src->idx;
    dst->size = src->size;
    dst->rsize = src->rsize;
    dst->hl_open_comment = src->hl_open_comment;
    dst->chars = malloc(src->size + 1);
    memcpy(dst->chars, src->chars, src->size + 1);
    dst->render = malloc(src->rsize + 1);
    memcpy(dst->render, src->render, src->rsize + 1);
    dst->hl = malloc(src->rsize);
    memcpy(dst->hl, src->hl, src->rsize);
  }
  return rows;
}

static void free_state(UndoState *s) {
  for (int i = 0; i < s->numrows; i++) {
    free(s->row[i].chars);
    free(s->row[i].render);
    free(s->row[i].hl);
  }
  free(s->row);
}

static void capture(UndoState *s) {
  s->row = clone_rows(E.row, E.numrows);
  s->numrows = E.numrows;
  s->cx = E.cx;
  s->cy = E.cy;
  s->rx = E.rx;
  s->rowoff = E.rowoff;
  s->coloff = E.coloff;
  s->dirty = E.dirty;
}

static void restore(const UndoState *s) {
  editorFreeRows();
  E.row = clone_rows(s->row, s->numrows);
  E.numrows = s->numrows;
  E.cx = s->cx;
  E.cy = s->cy;
  E.rx = s->rx;
  E.rowoff = s->rowoff;
  E.coloff = s->coloff;
  E.dirty = s->dirty;
  editorSelectSyntaxHighlight();
}

void editorUndoPush(void) {
  if (undo_len == UNDO_MAX) {
    free_state(&undo_stack[0]);
    memmove(&undo_stack[0], &undo_stack[1], sizeof(UndoState) * (UNDO_MAX - 1));
    undo_len--;
  }
  capture(&undo_stack[undo_len]);
  undo_len++;

  for (int i = 0; i < redo_len; i++) free_state(&redo_stack[i]);
  redo_len = 0;
}

void editorUndo(void) {
  if (undo_len == 0) {
    editorSetStatusMessage("Nothing to undo");
    return;
  }

  if (redo_len == UNDO_MAX) {
    free_state(&redo_stack[0]);
    memmove(&redo_stack[0], &redo_stack[1], sizeof(UndoState) * (UNDO_MAX - 1));
    redo_len--;
  }
  capture(&redo_stack[redo_len]);
  redo_len++;

  UndoState *s = &undo_stack[undo_len - 1];
  restore(s);
  free_state(s);
  undo_len--;

  editorSetStatusMessage("Undo");
}

void editorRedo(void) {
  if (redo_len == 0) {
    editorSetStatusMessage("Nothing to redo");
    return;
  }

  if (undo_len == UNDO_MAX) {
    free_state(&undo_stack[0]);
    memmove(&undo_stack[0], &undo_stack[1], sizeof(UndoState) * (UNDO_MAX - 1));
    undo_len--;
  }
  capture(&undo_stack[undo_len]);
  undo_len++;

  UndoState *s = &redo_stack[redo_len - 1];
  restore(s);
  free_state(s);
  redo_len--;

  editorSetStatusMessage("Redo");
}

void editorBeginUndoGroup(void) {
  if (undo_insert_group) return;
  undo_insert_group = 1;
  editorUndoPush();
}

void editorEndUndoGroup(void) {
  undo_insert_group = 0;
}
