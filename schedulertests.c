#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

char name[3];
int stdout = 1;

void ticks_running_test_with_process_does_not_exist()
{
    int pid = -1;
    int ticks;

    ticks = ticks_running(pid);

    if (ticks == -1)
        printf(stdout, "Ticks running test successful for a process that does not exists!\n");
    else
    {
        printf(stdout, "Ticks running test failed for a process that does not exists.\n");
        exit();
    }
}

void ticks_running_test_with_runnig_process()
{
    int pid;
    int ticks;

    pid = getpid();
    ticks = ticks_running(pid);

    if (ticks != -1)
        printf(stdout, "Ticks running test successful for a running process, number of ticks returned: %d.\n", ticks);
    else
    {
        printf(stdout, "Ticks running test failed for a running process, ticks returned: %d.\n", ticks);
        exit();
    }
}

void ticks_running_tests()
{
    ticks_running_test_with_process_does_not_exist();
    ticks_running_test_with_runnig_process();
    printf(stdout, "Ticks running tests OK.\n\n");
}

void stressfstest()
{
    int fd, i;
    char path[] = "stressfs0";
    char data[512];

    memset(data, 'a', sizeof(data));

    for (i = 0; i < 4; i++)
        if (fork() > 0)
            break;

    path[8] += i;
    fd = open(path, O_CREATE | O_RDWR);
    for (i = 0; i < 20; i++)
        write(fd, data, sizeof(data));
    close(fd);

    fd = open(path, O_RDONLY);
    for (i = 0; i < 20; i++)
        read(fd, data, sizeof(data));
    close(fd);

    wait();
}

void catandwritetest()
{
    int fd, fd1;
    char *filenametoread = "README";
    char *filenametowrite = "a.txt";
    char buf[512];
    int n;

    if ((fd = open(filenametoread, 0)) < 0)
    {
        printf(1, "cat: cannot open %s\n", filenametoread);
        exit();
    }

    unlink(filenametowrite);
    if ((fd1 = open(filenametowrite, O_CREATE | O_RDWR)) < 0)
    {
        printf(1, "cannot open file to write %s\n", filenametowrite);
        exit();
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0)
    {
        if (write(fd1, buf, n) != n)
        {
            printf(1, "cat: write error\n");
            exit();
        }
    }
    if (n < 0)
    {
        printf(1, "cat: read error\n");
        close(fd);
        close(fd1);
        exit();
    }
    unlink(filenametowrite);
    close(fd);
    close(fd1);
}

void functest()
{
    int i, fd;
    printf(1, "Child process 3 sleeping for 1000 ticks.\n");
    sleep(1000);
    for (i = 0; i < 200000; i++)
        ;

    // Create many files followed by unlink
    name[0] = 'a';
    name[2] = '\0';
    for (i = 0; i < 52; i++)
    {
        name[1] = '0' + i;
        fd = open(name, O_CREATE | O_RDWR);
        close(fd);
    }
    name[0] = 'a';
    name[2] = '\0';
    for (i = 0; i < 52; i++)
    {
        name[1] = '0' + i;
        unlink(name);
    }
}

void scheduler_performance_test()
{
    int pid, pid1, pid2, pid3, pid4;
    int pfds1[2], pfds2[2], pfds3[2], pfds4[2];
    int i, ptr = 0;
    int createdtime1 = -1, firstscheduledtime1 = -1, endtime1 = -1;
    int createdtime2 = -1, firstscheduledtime2 = -1, endtime2 = -1;
    int createdtime3 = -1, firstscheduledtime3 = -1, endtime3 = -1;
    int createdtime4 = -1, firstscheduledtime4 = -1, endtime4 = -1;

    printf(1, "Starting scheduler tests\n");
    printf(1, "Scheduling 4 child processes.\n");

    if (pipe(pfds1) == -1)
    {
        printf(1, "Pipe1 creation failure.\n");
        exit();
    }
    if (pipe(pfds2) == -1)
    {
        printf(1, "Pipe2 creation failure.\n");
        exit();
    }
    if (pipe(pfds3) == -1)
    {
        printf(1, "Pipe3 creation failure.\n");
        exit();
    }
    if (pipe(pfds4) == -1)
    {
        printf(1, "Pipe4 creation failure.\n");
        exit();
    }

    pid1 = fork();

    if (pid1 == 0)
    {
        for (i = 0; i < 100; i++)
            catandwritetest();

        pid = getpid();
        createdtime1 = get_created_time(pid);
        firstscheduledtime1 = get_first_scheduled_time(pid);
        endtime1 = uptime();

        close(pfds1[0]);
        if (write(pfds1[1], &createdtime1, sizeof(int)) == -1)
        {
            printf(1, "Writing Create time1 to pipe failed.\n");
            exit();
        }
        if (write(pfds1[1], &firstscheduledtime1, sizeof(int)) == -1)
        {
            printf(1, "Writing first scheduled time1 to pipe failed.\n");
            exit();
        }
        if (write(pfds1[1], &endtime1, sizeof(int)) == -1)
        {
            printf(1, "Writing end time1 to pipe failed.\n");
            exit();
        }
        exit();
    }
    else
    {
        pid2 = fork();
        if (pid2 == 0)
        {
            stressfstest();

            pid = getpid();
            createdtime2 = get_created_time(pid);
            firstscheduledtime2 = get_first_scheduled_time(pid);
            endtime2 = uptime();

            close(pfds2[0]);
            if (write(pfds2[1], &createdtime2, sizeof(int)) == -1)
            {
                printf(1, "Writing Create time2 to pipe failed.\n");
                exit();
            }
            if (write(pfds2[1], &firstscheduledtime2, sizeof(int)) == -1)
            {
                printf(1, "Writing first scheduled time2 to pipe failed.\n");
                exit();
            }
            if (write(pfds2[1], &endtime2, sizeof(int)) == -1)
            {
                printf(1, "Writing end time2 to pipe failed.\n");
                exit();
            }
            exit();
        }
        else
        {
            pid3 = fork();
            if (pid3 == 0)
            {
                functest();

                pid = getpid();
                createdtime3 = get_created_time(getpid());
                firstscheduledtime3 = get_first_scheduled_time(getpid());
                endtime3 = uptime();

                close(pfds3[0]);
                if (write(pfds3[1], &createdtime3, sizeof(int)) == -1)
                {
                    printf(1, "Writing Create time3 to pipe failed.\n");
                    exit();
                }
                if (write(pfds3[1], &firstscheduledtime3, sizeof(int)) == -1)
                {
                    printf(1, "Writing first scheduled time3 to pipe failed.\n");
                    exit();
                }
                if (write(pfds3[1], &endtime3, sizeof(int)) == -1)
                {
                    printf(1, "Writing end time3 to pipe failed.\n");
                    exit();
                }
                exit();
            }

            pid4 = fork();
            if (pid4 == 0)
            {
                for (i = 0; i < 150; i++)
                {
                    catandwritetest();
                }
                stressfstest();
                for (i = 0; i < 150000; i++)
                {
                    ptr++;
                }

                pid = getpid();
                createdtime4 = get_created_time(pid);
                firstscheduledtime4 = get_first_scheduled_time(pid);
                endtime4 = uptime();

                close(pfds4[0]);
                if (write(pfds4[1], &createdtime4, sizeof(int)) == -1)
                {
                    printf(1, "Writing Create time4 to pipe failed.\n");
                    exit();
                }
                if (write(pfds4[1], &firstscheduledtime4, sizeof(int)) == -1)
                {
                    printf(1, "Writing first scheduled time4 to pipe failed.\n");
                    exit();
                }
                if (write(pfds4[1], &endtime4, sizeof(int)) == -1)
                {
                    printf(1, "Writing end time4 to pipe failed.\n");
                    exit();
                }
                exit();
            }

            close(pfds1[1]);
            read(pfds1[0], &createdtime1, sizeof(int));
            read(pfds1[0], &firstscheduledtime1, sizeof(int));
            read(pfds1[0], &endtime1, sizeof(int));

            close(pfds2[1]);
            read(pfds2[0], &createdtime2, sizeof(int));
            read(pfds2[0], &firstscheduledtime2, sizeof(int));
            read(pfds2[0], &endtime2, sizeof(int));

            close(pfds3[1]);
            read(pfds3[0], &createdtime3, sizeof(int));
            read(pfds3[0], &firstscheduledtime3, sizeof(int));
            read(pfds3[0], &endtime3, sizeof(int));

            close(pfds4[1]);
            read(pfds4[0], &createdtime4, sizeof(int));
            read(pfds4[0], &firstscheduledtime4, sizeof(int));
            read(pfds4[0], &endtime4, sizeof(int));

            wait();
            wait();
            wait();
            wait();
            printf(1, "Child process 1 Created at: %d and First scheduled at: %d and Ended at: %d\n", createdtime1, firstscheduledtime1, endtime1);
            printf(1, "Child process 2 Created at: %d and First scheduled at: %d and Ended at: %d\n", createdtime2, firstscheduledtime2, endtime2);
            printf(1, "Child process 3 Created at: %d and First scheduled at: %d and Ended at: %d\n", createdtime3, firstscheduledtime3, endtime3);
            printf(1, "Child process 4 Created at: %d and First scheduled at: %d and Ended at: %d\n", createdtime4, firstscheduledtime4, endtime4);
        }
    }
    printf(1, "Scheduler Test OK.\n");
}

void fifo_test()
{
    int pid;
    int childposition, parentposition;
    int pfds[2];

    if (pipe(pfds) == -1)
    {
        printf(1, "Pipe creation failed.\n");
        exit();
    }

    pid = fork();

    if (pid == 0)
    {
        for (;;)
            ;
    }
    else
    {
        parentposition = fifo_position(getpid());
        childposition = fifo_position(pid);
        printf(1, "Parent: %d and child: %d\n", parentposition, childposition);
        if (parentposition < childposition)
        {
            printf(1, "Simple scheduler scheduling processes in FIFO order. Test Successful.\n");
        }
        else
        {
            printf(1, "Simple scheduler test failed.\n");
        }
        kill(pid);
        wait();
    }
}

void simple_scheduler_fifo_test()
{
    printf(1, "Simple Scheduler Test starting.\n");

#ifdef SCHEDULER_FIFO
    fifo_test();
#else
    printf(1, "OS not running on Simple scheduler FIFO implementation.\n");
#endif

    printf(1, "Simple Scheduler test OK.\n\n");
}

void lottery_test()
{
    int pid;
    int tickets;

    pid = getpid();

    tickets = get_lottery_tickets(pid);

    if (tickets == 150)
        printf(1, "Tickets for current process: %d is equal to default tickets.\n", tickets);
    printf(1, "Setting tickets of current process to 200.\n");

    set_lottery_tickets(200);

    tickets = get_lottery_tickets(pid);

    if (tickets == 200)
    {
        printf(1, "Tickets for current process set successfully.\n");
    }
    else
    {
        printf(1, "Set lottery tickets failed.\n");
    }
}

void advanced_scheduler_lottery_test()
{
    printf(1, "Advanced Scheduler Test starting.\n");
#ifdef SCHEDULER_LOTTERY
    lottery_test();
#else
    printf(1, "OS not running on Advanced scheduler Lottery implementation.\n");
#endif

    printf(1, "Advanced Scheduler test OK.\n\n");
}

void printscheduleralgorithm()
{
#ifdef SCHEDULER_FIFO
    printf(1, "OS is running on FIFO scheduling algorithm.\n");
#elif defined(SCHEDULER_LOTTERY)
    printf(1, "OS is running on Lottery scheduling algorithm.\n");
#elif defined(SCHEDULER_DEFAULT)
    printf(1, "OS is running on Default(Round Robin) scheduling algorithm.\n");
#else
    printf(1, "OS scheduling algorithm is not defined.\n");
#endif

    printf(1, "\n");
}

int main(int argc, char *argv[])
{
    printf(1, "schedulertests starting\n\n");
    printscheduleralgorithm();

    ticks_running_tests();
    simple_scheduler_fifo_test();
    advanced_scheduler_lottery_test();
    scheduler_performance_test();

    printf(1, "All scheduler tests passed.\n");

    exit();
}
