#include "micro.h"

/*** globals (defined here) ***/
struct editorConfig E;
Tab tabs[MICRO_MAX_TABS];
int tab_count = 1;
int cur_tab = 0;

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
