#include "types.h"
#include "user.h"

#define MAX_LINE_LENGTH 512

int strcmpwithlength(const char *p, const char *q, int l)
{
    while (*p && *p == *q && --l > 0)
        p++, q++;
    return (uchar)*p - (uchar)*q;
}

int min(int a, int b)
{
    return a < b ? a : b;
}

// Function to print unique lines based on the specified flags
void uniq(int fd, int countflag, int uniqflag, int widthflag)
{
    char currentline[MAX_LINE_LENGTH];
    char previousline[MAX_LINE_LENGTH] = "";

    int bytesread;
    int linestart = 0;
    int linecount = 0;
    int previouslinelength = MAX_LINE_LENGTH;
    int firstlineflag = 1;
    int minofprevandcurrentline;
    int comparisonlength;

    while ((bytesread = read(fd, currentline, sizeof(currentline))) > 0)
    {
        for (int i = 0; i < bytesread; i++)
        {
            if (currentline[i] == '\n')
            {
                currentline[i] = '\0';
                minofprevandcurrentline = min(previouslinelength, i - linestart);
                comparisonlength = widthflag ? min(widthflag, minofprevandcurrentline) : minofprevandcurrentline;

                if (firstlineflag == 1)
                {
                    firstlineflag = 0;
                    linecount = 1;
                    strcpy(previousline, currentline + linestart);
                }
                else
                {
                    if (strcmpwithlength(previousline, currentline + linestart, comparisonlength) == 0)
                    {
                        linecount++;
                    }
                    else
                    {
                        if (uniqflag)
                        {
                            if (linecount == 1)
                            {
                                if (countflag)
                                {
                                    printf(1, "%d ", linecount);
                                }
                                printf(1, "%s\n", previousline);
                            }
                        }
                        else
                        {
                            if (countflag)
                            {
                                printf(1, "%d ", linecount);
                            }
                            printf(1, "%s\n", previousline);
                        }
                        strcpy(previousline, currentline + linestart);
                        linecount = 1;
                        previouslinelength = i - linestart;
                    }
                }
                linestart = i + 1;
            }
        }
    }

    if (uniqflag)
    {
        if (linecount == 1)
        {
            if (countflag)
            {
                printf(1, "%d ", linecount);
            }
            printf(1, "%s\n", previousline);
        }
    }
    else
    {
        if (countflag)
        {
            printf(1, "%d ", linecount);
        }
        printf(1, "%s\n", previousline);
    }
}

int main(int argc, char *argv[])
{
    int fd;
    int countflag = 0;
    int uniqflag = 0;
    int widthflag = 0;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (argv[i][1] == 'c')
            {
                countflag = 1;
            }
            else if (argv[i][1] == 'u')
            {
                uniqflag = 1;
            }
            else if (argv[i][1] == 'w')
            {
                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    widthflag = atoi(argv[++i]);
                }
                else
                {
                    printf(2, "uniq: option requires an argument -- 'w'\n");
                    exit();
                }
            }
            else
            {
                printf(2, "uniq: invalid option -- '%c'\n", argv[i][1]);
                exit();
            }
        }
        else
        {
            if ((fd = open(argv[i], 0)) < 0)
            {
                printf(2, "uniq: cannot open file %s\n", argv[i]);
                exit();
            }
            uniq(fd, countflag, uniqflag, widthflag);
            close(fd);
        }
    }

    if (argc == 1)
    {
        uniq(0, countflag, uniqflag, widthflag);
    }

    exit();
}
