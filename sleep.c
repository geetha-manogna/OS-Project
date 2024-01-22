#include "types.h"
#include "user.h"


int
main(int argc, char *argv[])
{
    int sleepduration;

    //if too less or too many arguments are present
    if(argc != 2) {
        printf(2, "usage: sleep <ticks>\n");
        exit();
    }

    sleepduration = atoi(argv[1]);

    if(sleepduration < 0) {
        printf(2, "Number of ticks cannot be negative. Please provide a non-negative number");
        exit();
    }

    sleep(sleepduration); 
    exit();
}