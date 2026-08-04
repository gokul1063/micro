#include "micro.h"

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

static char *read_file_to_string(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *buf = malloc(len + 1);
  size_t got = fread(buf, 1, len, fp);
  buf[got] = '\0';
  fclose(fp);
  return buf;
}

static int editorTryRecover(const char *filename) {
  char *base = strdup(basename((char *)filename));
  char recpath[512];
  snprintf(recpath, sizeof(recpath), "~micro.%s.rec", base);
  free(base);

  if (access(recpath, F_OK) != 0) return 0;

  /* if the rec file matches the saved file, there is nothing to recover */
  char *fb = read_file_to_string(filename);
  char *rb = read_file_to_string(recpath);
  int recover = 0;
  if (fb && rb) {
    size_t rlen = strlen(rb);
    if (rlen > 0 && rb[rlen - 1] == '\n') rb[rlen - 1] = '\0';
    recover = (strcmp(fb, rb) != 0);
  } else {
    recover = 1;
  }
  free(fb);
  free(rb);
  if (!recover) return 0;

  char *ans = editorPrompt("Found %s. Recover unsaved changes? (y/n) %s", NULL);
  if (ans == NULL) {
    editorSetStatusMessage("Recovery cancelled");
    return 0;
  }
  char c = ans[0];
  free(ans);

  if (c != 'y' && c != 'Y') {
    remove(recpath);
    return 0;
  }

  FILE *fp = fopen(recpath, "r");
  if (!fp) return 0;

  E.filename = strdup(filename);
  editorSelectSyntaxHighlight();

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);

  E.dirty = 1;
  editorSetStatusMessage("Recovered unsaved changes");
  return 1;
}

void editorOpen(char *filename) {
  if (editorTryRecover(filename)) {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.mode = MODE_NORMAL;
    editorCreateRecFile();
    return;
  }

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
