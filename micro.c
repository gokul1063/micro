/** includes **/
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
/** data **/
struct termios orginal_attributes;

/** terminal **/
void die(const char *s){
  perror(s);
  exit(1);

}

void disableRawMode(){
  if (tcsetattr(STDIN_FILENO , TCSAFLUSH , &orginal_attributes) == -1)
    die("tcsetattr");

}

void enableRawMode(){

  struct termios raw;
  if (tcgetattr(STDIN_FILENO , &orginal_attributes) == -1 )
    die("tcgetatt");
  atexit(disableRawMode);
  raw = orginal_attributes;
  raw.c_iflag &= ~(ICRNL | IXON | INPCK | BRKINT | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 20;

  if (tcsetattr(STDIN_FILENO , TCSAFLUSH , &raw) == -1)
    die("tcsetattr");

}


/*** init ***/
int main() {
  enableRawMode();
  while (1){
    char c = '\0';
    if (read(STDIN_FILENO , &c , 1) == -1 && errno != EAGAIN) 
      die("read");
    if (iscntrl(c)){
      printf("%d\r\n" , c);
    }else {
      printf("%d (%c)\r\n" , c , c);

    }
    if (c == 'q')
        break;
  }
  return 0;


}
