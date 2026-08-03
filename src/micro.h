#ifndef MICRO_H
#define MICRO_H

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <libgen.h>

#include "config.h"

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL , 0}
#define MICRO_VERSION "1.2"
#define HL_HILIGHT_NUMBERS (1<<0)
#define HL_HILIGHT_STRING (1<<1)
#define MICRO_MAX_TABS 16

/*** data ***/
typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

struct editorConfig{
  int cx , cy ;
  int rx;
  int screenrows;
  int screencols;
  int coloff;
  int numrows;
  int rowoff;
  erow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orgin_termios;
  struct editorSyntax *syntax;
  EditorMode mode;
  char *clipboard;
  int clipboard_len;
  int visual_mode;        /* 0 = none, 1 = charwise, 2 = linewise */
  int visual_cy, visual_cx;
  char *recfile;
  int resize;
  int quit;
};

extern struct editorConfig E;

typedef struct {
  erow *row;
  int numrows;
  int cx, cy, rx;
  int rowoff, coloff;
  int dirty;
  char *filename;
  char *recfile;
  struct editorSyntax *syntax;
  EditorMode mode;
  int visual_mode;
  int visual_cy, visual_cx;
} Tab;

extern Tab tabs[MICRO_MAX_TABS];
extern int tab_count;
extern int cur_tab;

struct abuf{
  char* buffer;
  int len;
};

enum editorKey{
  BACKSPACE = 127,
  ARROW_LEFT = 1000 ,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,  
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
  ALT_TAB = 2000
};

enum editorHighlight{
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MCOMMENT,
  HL_kEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comments_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flag;
};

extern char *C_HL_extensions[];
extern char *C_HL_keywords[];
extern struct editorSyntax HLDB[];
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/
/* terminal.c */
void die(const char *s);
void enableRawMode(void);
void disableRawMode(void);
int editorReadKey(void);
int getCursorPosition(int *row , int *col);
int getWindowSize(int *row , int *col);
void editorHandleResize(void);
void editorInstallResizeHandler(void);

/* buffer.c */
int editorRowCxToRx(erow *row , int cx);
int editorRowRxToCx(erow *row , int rx);
void editorUpdateRow(erow *row);
void editorInsertRow(int at , char *s , size_t len);
void editorFreeRow(erow *row);
void editorFreeRows(void);
void editorDelRow(int at);
void editorRowInserChar(erow *row , int at , int c);
void editorRowAppendString(erow *row , char *s , size_t len);
void editorRowDelChar(erow *row, int at);
void editorInsertChar(int c);
void editorInsertNewline(void);
void editorDelChar(void);
char* editorRowsToString(int *buflen);
void editorCopyLine(void);
void editorPasteAfter(void);
void editorPasteBefore(void);
void editorDeleteLine(void);
void editorUndo(void);
void editorRedo(void);

/* undo.c */
void editorUndoPush(void);
void editorBeginUndoGroup(void);
void editorEndUndoGroup(void);

/* highlight.c */
int is_separator(int c);
void editorUpdateSyntax(erow *row);
RGB editorSyntaxToColor(int hl);
void editorSelectSyntaxHighlight(void);

/* fileio.c */
void editorCreateRecFile(void);
void editorWriteRecFile(void);
void editorRemoveRecFile(void);
char* editorReadRecFile(int *out_len);
void editorSave(void);
void editorOpen(char *filename);
void editorQuit(void);
void editorRequestQuit(void);

/* tabs.c */
void editorSaveTabState(void);
void editorLoadTabState(void);
void editorNewTab(void);
void editorNextTab(void);
void editorPrevTab(void);
void editorCloseTab(void);
void editorForceCloseTab(void);

/* find.c */
void editorFindCallback(char *query , int key);
void editorFind(int forward);
void editorSearchForward(void);
void editorSearchBackward(void);
void editorNextMatch(void);
void editorPrevMatch(void);

/* input.c */
char* editorPrompt(char* prompt , void (*callback)(char *, int));
void editorMoveCursor(int key);
void editorMoveWord(void);
void editorMoveWordEnd(void);
void editorMoveWordBack(void);
void editorProcessKey(void);
void editorSetMode(EditorMode mode);
void editorInsertMode(void);
void editorNormalMode(void);
void editorEnterVisualMode(int linewise);
void editorExitVisualMode(void);
void editorVisualModeProcessKey(int c);
void editorYankSelection(void);
void editorDeleteSelection(void);
void editorGotoLine(void);

/* screen.c */
void abAppend(struct abuf *ab , const char *s , int len);
void abFree(struct abuf *ab);
void abAppendFg(struct abuf *ab , RGB c);
void editorSetStatusMessage(const char *fmt , ...);
void editorScroll(void);
int editorLineNumWidth(void);
void editorDrawRows(struct abuf *ab);
void editorDrawStatusBar(struct abuf *ab);
void editorDrawMessageBar(struct abuf *ab);
void editorRefreshScreen(void);

/* main.c */
void initEditor(void);

#endif
