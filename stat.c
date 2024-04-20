#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
stat_helper(char *path)
{
  int fd;
  struct stat st;
  uint i = 0;

  if((fd = open(path, 0)) < 0){
    printf(2, "stat: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "stat: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
    case T_DIR: {
        printf(1, "Given path is a Directory.\n");
        printf(1, "Directory INode details--> Type: Directory, Size: %d bytes.\n", st.size);
        printf(1, "INode memory addresses of %d data blocks: \n", INODEADDRSSIZE);
        for(i=0;i<INODEADDRSSIZE;i++) {
            printf(1, "Block %d address: %d\n", i+1, st.addrs[i]);
        }
        break;
    }
    case T_FILE: {
        printf(1, "Given path is a Pointers based file.\n");
        printf(1, "File INode details--> Type: Pointers based file, Size: %d bytes.\n", st.size);
        printf(1, "INode memory addresses of %d data blocks: \n", INODEADDRSSIZE);
        for(i=0;i<INODEADDRSSIZE;i++) {
            printf(1, "Block %d address: %d\n", i+1, st.addrs[i]);
        }
        break;
    }
    case T_EXTENT: {
        printf(1, "Given path is a Extent based file.\n");
        printf(1, "File INode details--> Type: Extent based file, Size: %d bytes.\n", st.size);
        printf(1, "INode memory addresses and length of %d data block pointers: \n", INODEADDRSSIZE/2);
        for(i=0;i<2*(INODEADDRSSIZE/2);i+=2) {
            printf(1, "Pointer %d starting address: %d and extent length: %d\n", i/2+1, st.addrs[i], st.addrs[i+1]);
        }
        break;
    }
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    stat_helper(".");
    exit();
  }
  for(i=1; i<argc; i++)
    stat_helper(argv[i]);
  exit();
}
