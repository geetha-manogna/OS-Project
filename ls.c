#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

char*
fmtname(char *path, int isdirectorypath)
{
  static char buf[DIRSIZ+1];
  char *p;
  int i;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(isdirectorypath) {
    i = strlen(p);
    p[i] = '/';
    p[i+1] = 0;
  }

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void printfmtname(char* path, struct stat *st, int showhiddencontents) {
  int ishiddenfileordirectory;
  
  char* printablename = fmtname(path, st->type == T_DIR);

  ishiddenfileordirectory = printablename[0] == '.';

  if(!ishiddenfileordirectory || showhiddencontents) {
      printf(1, "%s %d %d %d\n", printablename, st->type, st->ino, st->size);
  }
}

void
ls(char *path, int showhiddencontents)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printfmtname(path, &st, showhiddencontents);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
      printfmtname(buf, &st, showhiddencontents);
    }
    break;
  }
  close(fd);
}


int
doesargscontainflag(int argc, char *argv[], char *flag) {
  int i;
  for(i=1;i<argc;i++) {
    if(!strcmp(argv[i], flag)) {
      return 1;
    }
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int i;
  char* showhiddencontentsflag = "-a";
  int showhiddencontents;

  if(argc < 2){
    ls(".", 0);
    exit();
  }

  showhiddencontents = doesargscontainflag(argc, argv, showhiddencontentsflag);

  if(showhiddencontents && argc == 2) {
    ls(".", 1);
    exit();
  }

  for(i=1; i<argc; i++) {
    if(strcmp(argv[i], showhiddencontentsflag)) {
      ls(argv[i], showhiddencontents);
    }
  }

  exit();
}
