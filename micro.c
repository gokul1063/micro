/** includes **/
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
#define MICRO_TAB_STOP 8
#define MICRO_QUIT_TIMES 3
#define HL_HILIGHT_NUMBERS (1<<0)
#define HL_HILIGHT_STRING (1<<1)


/** data **/

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
  int visual_mode;        // 0 = none, 1 = charwise, 2 = linewise
  int visual_cy, visual_cx; // selection anchor
  char *recfile;          // path of the ~micro.<name>.rec recovery file
  int resize;             // set when SIGWINCH received
  int quit;               // set when user wants to quit
};

struct editorConfig E;

#define MICRO_MAX_TABS 16

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

Tab tabs[MICRO_MAX_TABS];
int tab_count = 1;
int cur_tab = 0;


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



/*** filetypes ***/

char *C_HL_extensions[] = {".c" , ".h" , ".cpp" , NULL};
char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

struct editorSyntax HLDB[] = {
  {
    "c",
    C_HL_extensions,
    C_HL_keywords,
    "//",
    "/*",
    "*/",
    HL_HILIGHT_NUMBERS | HL_HILIGHT_STRING
  },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt , ...);
void editorRefreshScreen();
char* editorPrompt(char* prompt , void (*callback)(char *, int));

void editorSetMode(EditorMode mode);
void editorInsertMode(void);
void editorNormalMode(void);
void editorCopyLine(void);
void editorPasteAfter(void);
void editorPasteBefore(void);
void editorDeleteLine(void);
void editorUndo(void);
void editorRedo(void);
void editorEnterVisualMode(int linewise);
void editorExitVisualMode(void);
void editorVisualModeProcessKey(int c);
void editorYankSelection(void);
void editorDeleteSelection(void);
void editorGotoLine(void);
void editorSearchForward(void);
void editorSearchBackward(void);
void editorNextMatch(void);
void editorPrevMatch(void);

void editorCreateRecFile(void);
void editorWriteRecFile(void);
void editorRemoveRecFile(void);
char* editorReadRecFile(int *out_len);
void editorHandleResize(void);
void editorQuit(void);
void editorRequestQuit(void);
void editorCloseTab(void);
void editorSave(void);
void editorFreeRows(void);
void editorSaveTabState(void);
void editorLoadTabState(void);
void editorNewTab(void);
void editorNextTab(void);
void editorPrevTab(void);
void editorInstallResizeHandler(void);
int getWindowSize(int *row , int *col);
int editorLineNumWidth(void);

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


/** terminal **/
void die(const char *s){

  write(STDOUT_FILENO , "\x1b[2J" , 4);
  write(STDOUT_FILENO , "\x1b[H" , 3);

  perror(s);
  exit(1);

}

void disableRawMode(){
  if (tcsetattr(STDIN_FILENO , TCSAFLUSH , &E.orgin_termios) == -1)
    die("tcsetattr");

}

void enableRawMode(){

  struct termios raw;
  if (tcgetattr(STDIN_FILENO , &E.orgin_termios) == -1 )
    die("tcgetatt");
  atexit(disableRawMode);
  raw = E.orgin_termios;
  raw.c_iflag &= ~(ICRNL | IXON | INPCK | BRKINT | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); 
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 20;

  if (tcsetattr(STDIN_FILENO , TCSAFLUSH , &raw) == -1)
    die("tcsetattr");

}

static void onResizeSignal(int sig) {
  (void)sig;
  E.resize = 1;
}

void editorHandleResize(void) {
  if (!E.resize) return;
  E.resize = 0;

  int rows = 0, cols = 0;
  if (getWindowSize(&rows, &cols) == -1)
    die("getWindowSize");

  E.screenrows = rows - 2;
  E.screencols = cols;

  if (E.rowoff > 0 && E.cy < E.rowoff) E.rowoff = E.cy;
  if (E.coloff > 0 && E.rx < E.coloff) E.coloff = E.rx;

  editorRefreshScreen();
}

void editorInstallResizeHandler(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = onResizeSignal;
  sigaction(SIGWINCH, &sa, NULL);
}


int editorReadKey(){
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO , &c , 1)) != 1){
    if (nread == -1 && errno == EINTR){
      E.resize = 1;
      return -1;
    }
    if (nread == -1 && errno != EAGAIN) 
        die("read");
  }


  if (c == '\x1b'){
    char seq[3];

    if (read(STDIN_FILENO , &seq[0] , 1) != 1) return '\x1b';

    if (seq[0] == '['){
      if (read(STDIN_FILENO , &seq[1] , 1) != 1) return '\x1b';
      if (seq[1] >= '0' && seq[1] <= '9'){
        if (read(STDIN_FILENO , &seq[2] , 1) != 1) return '\x1b';
        if (seq[2] == '~'){
          switch(seq[1]){
            case '1' : return HOME_KEY;
            case '3' : return DEL_KEY; 
            case '4' : return END_KEY;
            case '5' : return PAGE_UP;
            case '6' : return PAGE_DOWN;
            case '7' : return HOME_KEY;
            case '8' : return END_KEY;
          }
        }
      } else {
        switch (seq[1]){
        case 'A' : return ARROW_UP;
        case 'B' : return ARROW_DOWN;
        case 'C' : return ARROW_RIGHT;
        case 'D' : return ARROW_LEFT;
        case 'H' : return HOME_KEY;
        case 'F' : return END_KEY;
        }
      }
    } else if (seq[0] == 'O'){
      if (read(STDIN_FILENO , &seq[1] , 1) != 1) return '\x1b';
      switch (seq[1]) {
        case 'H' : return HOME_KEY;
        case 'F' : return END_KEY;
      }
    } else if (seq[0] == '\t'){
      /* Alt+Tab */
      return ALT_TAB;
    }

    return '\x1b';
  } else {
    return c;
  }
}


int getCursorPosition(int *row , int *col){
  char buff[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO , "\x1b[6n" , 4) != 4) 
      return -1;
  while (i < sizeof(buff) - 1){
    if (read(STDIN_FILENO , &buff[i] , 1) != 1)
      break;
    if (buff[i] =='R')
      break;
    i++;
  }

  buff[i] = '\0';


  if (buff[0] != '\x1b' || buff[1] != '[') return -1;
  if (sscanf(&buff[2] , "%d;%d" , row , col) != 2) return -1;

  return 0;
}

int getWindowSize(int *row , int *col){
  struct winsize ws;

  if (ioctl(STDOUT_FILENO , TIOCGWINSZ , &ws) == -1 || ws.ws_col == 0){
    if (write(STDOUT_FILENO , "\x1b[999C\x1b[999B" , 12) != 12)
      return -1;
    return getCursorPosition(row, col);
  }
  else {
    *col = ws.ws_col;
    *row = ws.ws_row;
    return 0;
  }

}

/*** syntax highlighting ***/


int is_separator(int c){
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];" , c) != NULL;
}


void editorUpdateSyntax(erow *row){
  row->hl = realloc(row->hl , row->rsize);
  memset(row->hl , HL_NORMAL , row->rsize);

  
  if (E.syntax == NULL) return;

  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
  int prev_sep = 1;
  int in_string = 0;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->singleline_comments_start;
  int scs_len = scs ? strlen(scs) : 0;

  // FOR MULTILINE COMMENT
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;


  int i= 0;
  while(i < row->rsize){
    char c = row->render[i];

    unsigned char prev_hl = (i > 0) ? row->hl[i-1] : HL_NORMAL;

    if (scs_len && !in_string && !in_comment){
      if (!strncmp(&row->render[i] , scs , scs_len) ){
        memset(&row->hl[i] , HL_COMMENT , row->rsize - i);
        break;
      }
    }

    if (mcs_len && mce_len && !in_string){
      if (in_comment){
        row->hl[i] = HL_MCOMMENT;
        if (!strncmp(&row->render[i] , mce , mce_len)) {
          memset(&row->hl[i] , HL_MCOMMENT , mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;

        } else {
          i ++;
          continue;
        }
      } else if (!strncmp(&row->render[i] , mcs , mcs_len)){
          memset(&row->hl[i] , HL_MCOMMENT , mcs_len );
          i += mcs_len;
          in_comment = 1;
          continue;
        }
    }

    if (E.syntax->flag & HL_HILIGHT_NUMBERS){

      if (in_string){
        row->hl[i] = HL_STRING;
        if (c == '\\' && i +1 < row->rsize){
          row->hl[ i+1 ] = HL_STRING;
          i += 2;
          continue;
        }

        if (c == in_string) in_string = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\''){
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;

        }
      }

      if ( ( isdigit(c) && (prev_sep || prev_hl == HL_NUMBER) ) || (c == '.' && prev_hl == HL_NUMBER ) ){
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep){
      int j;
      for (j = 0 ; keywords[j] ; j++){
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen-1] == '|';
        if (kw2) klen--;


        if (!strncmp(&row->render[i] , keywords[j] , klen) && 
            (is_separator(row->render[i+klen]) ) ){
          memset(&row->hl[i],kw2 ? HL_KEYWORD2 : HL_kEYWORD1, klen);
          i += klen;
          break;
        }
      }

      if (keywords[j] != NULL){
        prev_sep = 0;
        continue;
      }
    }

    prev_sep = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;

  if (changed && row->idx + 1 < E.numrows){
    editorUpdateSyntax(&E.row[row->idx + 1]);
  }
}



RGB editorSyntaxToColor(int hl){
  EditorConfig *cfg = config_get();
  ColorTheme *colors = &cfg->colors;
  switch(hl){
    case HL_NUMBER : return colors->number;
    case HL_kEYWORD1 : return colors->keyword1;
    case HL_KEYWORD2 : return colors->keyword2;                       
    case HL_MCOMMENT : return colors->multiline_comment;
    case HL_COMMENT : return colors->comment;
    case HL_STRING : return colors->string;
    case HL_MATCH : return colors->match;
    default: return colors->foreground;
  }
}

void abAppendFg(struct abuf *ab , RGB c){
  char buf[32];
  int len = snprintf(buf , sizeof(buf) , "\x1b[38;2;%d;%d;%dm" , c.r , c.g , c.b);
  abAppend(ab , buf , len);
}


void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) return;
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
        for(filerow = 0 ; filerow < E.numrows ; filerow++){
          editorUpdateSyntax(&E.row[filerow]);
        }

        return;
      }
      i++;
    }
  }


}


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
    // editorMoveCursor(ARROW_LEFT);
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1] , row->chars , row->size);
    editorDelRow(E.cy);
    E.cy --;
  }

}
/*** file i/0 ***/
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


void editorCreateRecFile(void) {
  if (!E.filename) return;
  if (E.recfile) {
    free(E.recfile);
    E.recfile = NULL;
  }

  char *base = strdup(basename((char *)E.filename));
  size_t need = strlen(base) + 16;
  E.recfile = malloc(need);
  snprintf(E.recfile, need, "~micro.%s.rec", base);
  free(base);

  editorWriteRecFile();
}

void editorWriteRecFile(void) {
  if (!E.recfile) return;

  int len;
  char *buff = editorRowsToString(&len);
  int fd = open(E.recfile, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd == -1) {
    free(buff);
    return;
  }
  if (write(fd, buff, len) != len) {
    /* ignore partial write for recovery file */
  }
  close(fd);
  free(buff);
}

void editorRemoveRecFile(void) {
  if (E.recfile) {
    remove(E.recfile);
    free(E.recfile);
    E.recfile = NULL;
  }
}

void editorQuit(void) {
  for (int i = 0; i < tab_count; i++) {
    if (tabs[i].recfile) {
      remove(tabs[i].recfile);
      free(tabs[i].recfile);
      tabs[i].recfile = NULL;
      if (i == cur_tab) E.recfile = NULL;
    }
  }
  if (E.recfile) {
    remove(E.recfile);
    free(E.recfile);
    E.recfile = NULL;
  }
  write(STDOUT_FILENO , "\x1b[2J" , 4);
  write(STDOUT_FILENO , "\x1b[H" , 3);
  exit(0);
}

void editorRequestQuit(void) {
  if (!E.dirty) {
    editorQuit();
    return;
  }

  char *answer = editorPrompt("Unsaved changes. Save before quitting? (y/n) %s", NULL);
  if (answer == NULL) {
    editorSetStatusMessage("Quit cancelled");
    return;
  }

  char c = answer[0];
  free(answer);

  if (c == 'y' || c == 'Y') {
    editorSave();
    if (E.dirty == 0) {
      editorQuit();
    } else {
      editorSetStatusMessage("Quit cancelled");
    }
  } else if (c == 'n' || c == 'N') {
    editorQuit();
  } else {
    editorSetStatusMessage("Quit cancelled");
  }
}

void editorCloseTab(void) {
  if (E.dirty) {
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

char* editorReadRecFile(int *out_len) {
  if (!E.recfile) return NULL;

  FILE *fp = fopen(E.recfile, "r");
  if (!fp) return NULL;

  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *buff = malloc(len + 1);
  size_t got = fread(buff, 1, len, fp);
  buff[got] = '\0';
  fclose(fp);

  *out_len = (int)got;
  return buff;
}

void editorSave(){
  if (E.filename == NULL){
    E.filename = editorPrompt("Save as: %s" , NULL);
    if (E.filename == NULL){
      editorSetStatusMessage("Save aborted");
      return;
    }

    editorSelectSyntaxHighlight();
    editorCreateRecFile();
  }

  /* The recovery file always holds the latest buffer state. */
  if (!E.recfile) editorCreateRecFile();
  editorWriteRecFile();

  int len;
  char *buff = editorReadRecFile(&len);
  if (buff == NULL) {
    buff = editorRowsToString(&len);
  }

  int fd = open(E.filename , O_RDWR | O_CREAT , 0644);

  if (fd != -1){
    if(ftruncate(fd , len) != -1){
      if (write(fd,buff,len) == len){
        close(fd);
        free(buff);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      } else {
        die("write-file");
      }
    } else {
      close(fd);
      die("ftruncate");
    }
  } else {
    die("fopen");
  }

  free(buff);
  editorSetStatusMessage("can't save! I/O error :%s" , strerror(errno));

}


void editorOpen(char *filename) {
  free(E.filename);
  if (E.recfile) {
    remove(E.recfile);
    free(E.recfile);
    E.recfile = NULL;
  }
  editorFreeRows();
  FILE *fp = fopen(filename , "r");

  if (!fp && errno == ENOENT) {
    /* file does not exist: create an empty one */
    int fd = open(filename , O_RDWR | O_CREAT , 0644);
    if (fd == -1) die("open");
    close(fd);
    fp = fopen(filename , "r");
  }

  if (!fp) die("fopen");


  E.filename = strdup(filename);
  
  editorSelectSyntaxHighlight();

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen; 
  while((linelen = getline(&line , &linecap , fp)) != -1){
    while(linelen > 0 && (line[linelen - 1] == '\n'||
                          line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows , line , linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.mode = MODE_NORMAL;
  E.visual_mode = 0;
  E.visual_cy = 0;
  E.visual_cx = 0;

  editorCreateRecFile();
}

/*** tabs ***/

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

/*** find ***/

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

    switch (c) {
      case '\r':
        editorInsertMode();
        editorInsertNewline();
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
          editorInsertNewline();
          editorInsertMode();
        } else if (c == insert_above_key) {
          if (E.cy > 0) { E.cy--; }
          E.cx = 0;
          editorInsertNewline();
          editorMoveCursor(ARROW_UP);
          editorInsertMode();
        } else if (c == delete_char_key) {
          editorDelChar();
        } else if (c == delete_line_key) {
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
          editorPasteAfter();
        } else if (c == paste_before_key) {
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

void editorSetStatusMessage(const char *fmt , ...){
  va_list ap;
  va_start(ap , fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg) , fmt , ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

void editorSetMode(EditorMode mode) {
  E.mode = mode;
}

void editorInsertMode(void) {
  E.mode = MODE_INSERT;
  editorSetStatusMessage("-- INSERT --");
}

void editorNormalMode(void) {
  E.mode = MODE_NORMAL;
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
    editorDeleteSelection();
    editorExitVisualMode();
  } else if (c == '\x1b' || c == 'v' || c == 'V') {
    editorExitVisualMode();
  }
  // else ignore other keys
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
      // now first line is at start_y, next line is what was after end_y
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

void editorUndo(void) {
  editorSetStatusMessage("Undo not implemented yet");
}

void editorRedo(void) {
  editorSetStatusMessage("Redo not implemented yet");
}

void editorGotoLine(void) {
  char *cmd = editorPrompt(":%s", NULL);
  if (cmd == NULL) return;

  char *p = cmd;
  while (*p == ' ') p++;

  if (*p == 'e') {
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

int editorLineNumWidth(void) {
  EditorConfig *cfg = config_get();
  if (!cfg->settings.show_line_numbers) return 0;
  int n = E.numrows > 0 ? E.numrows : 1;
  int digits = 1;
  while (n >= 10) { n /= 10; digits++; }
  return digits + 1; /* digits + trailing space */
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



/*** init ***/
void initEditor() {
  E.cx = 0;
  E.rx = 0;
  E.cy = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.rowoff = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.dirty = 0;
  E.syntax = NULL;
  E.mode = MODE_NORMAL;
  E.clipboard = NULL;
  E.clipboard_len = 0;
  E.visual_mode = 0;
  E.visual_cy = 0;
  E.visual_cx = 0;
  E.recfile = NULL;
  E.resize = 0;
  E.quit = 0;

  config_init();
  config_load("config.json");

  if (getWindowSize(&E.screenrows , &E.screencols ) == -1 )
    die("getWindowSize");
  E.screenrows -= 2;

}

int main(int argc , char *argv[]) {
  enableRawMode();
  editorInstallResizeHandler();
  initEditor();
  if (argc >= 2){
    editorOpen(argv[1]);
    write(STDOUT_FILENO, "\x1b[1 q" , 5);
  }

  editorSetStatusMessage("i=insert | Tab=new tab | Alt+Tab=prev | :e <file>=open | :<n>=goto | Ctrl-s=save | Ctrl-q=quit");
  while(1){
    editorHandleResize();
    if (E.dirty) editorWriteRecFile();
    if (E.quit) break;
    editorRefreshScreen();
    editorProcessKey();
  }
  return 0;


}
