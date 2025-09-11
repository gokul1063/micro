/** includes **/
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL , 0}
#define MICRO_VERSION "0.0.1"

/** data **/
struct editorConfig{
  int cx , cy ;
  int screenrows;
  int screencols;
  struct termios orgin_termios;
};

struct editorConfig E;


struct abuf{
  char* buffer;
  int len;
};


enum editorKey{
  ARROW_lEFT = 1000 ,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,  
  PAGE_UP,
  PAGE_DOWN
};


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


int editorReadKey(){
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO , &c , 1)) != 1){
    if (nread == -1 && errno != EAGAIN) 
        die("read");
  }


  if (c == '\x1b'){
    char seq[3];

    
    if (read(STDIN_FILENO , &seq[0] , 1) != 1) return '\x1b';
    if (read(STDIN_FILENO , &seq[1] , 1) != 1) return '\x1b';

    if (seq[0] == '['){
      if (seq[1] >= '0' && seq[1] <= '9'){
        if (read(STDIN_FILENO , &seq[2] , 1) != 1) return '\x1b';
        if (seq[2] == '~'){
          switch(seq[1]){
            case '5' : return PAGE_UP;
            case '6' : return PAGE_DOWN;

          }
        }


      } else {
        switch (seq[1]){
        case 'A' : return ARROW_UP;
        case 'B' : return ARROW_DOWN;
        case 'C' : return ARROW_RIGHT;
        case 'D' : return ARROW_lEFT;
        }
      }
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




/*** input ***/
void editorMoveCursor(int key){
  switch (key){
    case ARROW_lEFT :
      (E.cx - 1 < 0) ? E.cx : E.cx --;
      break;
    case ARROW_RIGHT:
      (E.cx + 1 > E.screencols) ? E.cx : E.cx ++;
      break;
    case ARROW_UP:
      (E.cy - 1 < 0) ? E.cy : E.cy --;
      break;
    case ARROW_DOWN:
      (E.cy + 1 > E.screenrows) ? E.cy : E.cy ++;
      break;
  }
}

void editorProcessKey(){
  int c = editorReadKey();

  switch (c){
    case CTRL_KEY('q'):
      write(STDOUT_FILENO , "\x1b[2J" , 4);
      write(STDOUT_FILENO , "\x1b[H" , 3);
      exit(0);
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times --) 
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_DOWN:
    case ARROW_UP:
    case ARROW_RIGHT:
    case ARROW_lEFT:
      editorMoveCursor(c);
      break;
  }

}


/*** output ***/

void editorDrawRows(struct abuf *ab){
  int y;
  for (y = 0 ; y < E.screenrows ; y++){
    if (y == E.screenrows /3){
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

    } else {
      abAppend(ab , "~",1);
    }
    
    abAppend(ab , "\x1b[K" , 3);
    if (y < E.screenrows-1){
      abAppend(ab,"\r\n", 2);
    }
  }


}

void editorRefreshScreen(){
  struct abuf ab = ABUF_INIT;

  abAppend(&ab , "\x1b[?25l" , 6); // HIDES THE CURSSOR
  abAppend(&ab , "\x1b[H" , 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf , sizeof(buf) , "\x1b[%d;%dH" , E.cy + 1 , E.cx + 1);
  abAppend(&ab , buf , strlen(buf));

  
  abAppend(&ab , "\x1b[?25h" , 6);
  write(STDOUT_FILENO , ab.buffer , ab.len);
  abFree(&ab);
}


/*** init ***/
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  if (getWindowSize(&E.screenrows , &E.screencols ) == -1 )
    die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();
  while(1){
    editorRefreshScreen();
    editorProcessKey();
  }
  return 0;


}
