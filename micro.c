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


char editorReadKey(){
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO , &c , 1)) != 1){
    if (nread == -1 && errno != EAGAIN) 
        die("read");
  }

  return c;
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
void editorMoveCursor(char key){
  switch (key){
    case 'a':
      E.cx --;
      break;
    case 'd':
      E.cx ++;
      break;
    case 'w':
      E.cy--;
      break;
    case 's':
      E.cy++;
      break;
  }
}

void editorProcessKey(){
  char c = editorReadKey();

  switch (c){
    case CTRL_KEY('q'):
      write(STDOUT_FILENO , "\x1b[2J" , 4);
      write(STDOUT_FILENO , "\x1b[H" , 3);
      exit(0);
    case 'w':
    case 'a':
    case 's':
    case 'd':
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
