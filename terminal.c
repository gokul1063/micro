#include "micro.h"

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
