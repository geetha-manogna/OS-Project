#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

// char buf[8192];
char name[3];
int stdout = 1;
#define NULL ((void *)0)
char *echoargv[] = { "echo", "ALL", "TESTS", "PASSED", NULL};


void ticks_running_test_with_process_does_not_exist() {
    int pid = -1;
    int ticks;
    
    ticks = ticks_running(pid);

    if (ticks == -1) 
      printf(stdout, "Ticks running test successful for a process that does not exists!\n");
    else {
      printf(stdout, "Ticks running test failed for a process that does not exists.\n");
      exit();
    }
}

void ticks_running_test_with_runnig_process() {
    int pid;
    int ticks;
    
    pid = getpid();
    ticks = ticks_running(pid);

    if (ticks != -1) 
      printf(stdout, "Ticks running test successful for a running process, number of ticks returned: %d.\n", ticks);
    else {
      printf(stdout, "Ticks running test failed for a running process, ticks returned: %d.\n", ticks);
      exit();
    }
}

void ticks_running_tests() {
  ticks_running_test_with_process_does_not_exist();
  ticks_running_test_with_runnig_process();
  printf(stdout, "Ticks running tests OK.\n");
}

void stressfsexec()
{
    char *stressfsargs[1];
    exec("stressfs", stressfsargs);
}

void test_scheduler_performance()
{
    int pid;
    int i;
    int createdtime1=-1, firstscheduledtime1=-1;
    int createdtime2 =-1, firstscheduledtime2 =-1;
    char *findargs[4];
    printf(1, "Starting scheduler tests\n");
    
    pid = fork();

    if(pid==0) {
        for(i=0;i<1000;i++);
        findargs[0] = "find";
        findargs[1] = ".";
        findargs[2] = "-name";
        findargs[3] = "find";
        char *args[] = {"sh", "-c", "echo abc > a.txt", NULL};
        exec("sh", args);

        exec("echo", findargs);
        exec("echo", findargs);
        exec("find", findargs);
        exec("find", findargs);
        stressfsexec();
        // stressfsexec();
        createdtime2 = get_created_time(getpid());
        firstscheduledtime2 = get_first_scheduled_time(getpid());
        printf(1, "Child process created time: %d and first scheduled time: %d\n", createdtime2, firstscheduledtime2);
    } else {
        createdtime1 = get_created_time(getpid());
        firstscheduledtime1 = get_first_scheduled_time(getpid());
        for(i=0;i<2000;i++);
        printf(1, "Parent process created time: %d and first scheduled time: %d\n", createdtime1, firstscheduledtime1);
        
    }

    wait();
}

int
main(int argc, char *argv[])
{
  printf(1, "schedulertests starting\n");

  ticks_running_tests();
  test_scheduler_performance();

  exit();
}
