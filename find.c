#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

void find(char *entrypath, char *filename, short filetypeoption, int inumflaginput, int inumflagcomparisonoperator, int isprintiflagpresent)
{
    char *p;
    int fd;
    struct dirent de;
    struct stat st;
    char paths[24][128];
    int i = 0;
    int sizeofpathsarray = 1;
    char currentdirectorypath[128];

    strcpy(paths[0], entrypath);

    for (i = 0; i < sizeofpathsarray; i++)
    {
        strcpy(currentdirectorypath, paths[i]);

        if ((fd = open(currentdirectorypath, 0)) < 0)
        {
            printf(2, "Cannot open directory: %s\n", currentdirectorypath);
            return;
        }

        if (fstat(fd, &st) < 0)
        {
            printf(2, "Cannot stat directory: %s\n", currentdirectorypath);
            close(fd);
            return;
        }

        p = currentdirectorypath + strlen(currentdirectorypath);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if (stat(currentdirectorypath, &st) < 0)
            {
                printf(2, "find: cannot stat %s\n", currentdirectorypath);
                continue;
            }

            if (strcmp(de.name, filename) == 0)
            {
                if ((filetypeoption == 0) || (filetypeoption == st.type))
                {
                    if (inumflaginput == 0 ||
                    (inumflagcomparisonoperator == 0 && st.ino == inumflaginput) || 
                    (inumflagcomparisonoperator == -1 && st.ino < inumflaginput) || 
                    (inumflagcomparisonoperator == 1 && st.ino > inumflaginput))
                    {
                        if (isprintiflagpresent)
                        {
                            printf(1, "%d ", st.ino);
                        }
                        printf(1, "%s\n", currentdirectorypath);
                    }
                }
            }

            if (st.type == 1)
            {
                strcpy(paths[sizeofpathsarray++], currentdirectorypath);
            }
        }
        close(fd);
    }
}

int main(int argc, char *argv[])
{
    int i;

    /*All flags*/
    char *nameflag = "-name";
    char *typeflag = "-type";
    char *inumflag = "-inum";
    char *printiflag = "-printi";

    char foldername[DIRSIZ];
    char filename[DIRSIZ];
    short filetypeoption = 0; /* 1 for directory(T_DIR), 2 for file(T_FILE), 0 for both */
    int inumflaginput = 0;
    int inumflagcomparisonoperator = 0; /* 0 for exact match, 1 for greater than comparison, -1 for less than comparison*/
    int isprintiflagpresent = 0;

    if (argc < 4)
    {
        printf(2, "find: format find <folder_name> -name <file_name>. Available flags are type, printi, inum.\n");
        exit();
    }

    strcpy(foldername, argv[1]);

    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], typeflag) == 0)
        {
            i++;
            if (strcmp(argv[i], "f") == 0)
            {
                filetypeoption = 2;
            }
            else
            {
                filetypeoption = 1;
            }
        }
        else if (strcmp(argv[i], inumflag) == 0)
        {
            i++;
            if (argv[i][0] == '+')
            {
                inumflaginput = atoi(++argv[i]);
                inumflagcomparisonoperator = 1;
            }
            else if (argv[i][0] == '-')
            {
                inumflaginput = atoi(++argv[i]);
                inumflagcomparisonoperator = -1;
            }
            else
            {
                inumflaginput = atoi(argv[i]);
            }
        }
        else if (strcmp(argv[i], nameflag) == 0)
        {
            i++;
            strcpy(filename, argv[i]);
        }
        else if (strcmp(argv[i], printiflag) == 0)
        {
            isprintiflagpresent = 1;
        }
        else
        {
            printf(2, "Invalid input/flag: %s\n");
            exit();
        }
    }

    find(foldername, filename, filetypeoption, inumflaginput, inumflagcomparisonoperator, isprintiflagpresent);

    exit();
}
