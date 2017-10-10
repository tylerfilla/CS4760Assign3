/*
 * Tyler Filla
 * CS 4760
 * Assignment 3
 */

//
// clock.c
// This is the main file for the clock library.
//

#include <errno.h>
#include <stdio.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include "clock.h"

#define FTOK_PATH "/bin/echo"
#define FTOK_CHAR 'C'

struct __clock_mem_s
{
    /** Nanosecond counter. */
    int nanos;

    /** Second counter. */
    int seconds;
};

/**
 * Start the clock under IN mode.
 */
static int clock_start_in(clock_s* self)
{
    if (self->running)
        return -100;

    errno = 0;

    // Obtain the IPC key
    key_t key = ftok(FTOK_PATH, FTOK_CHAR);
    if (errno)
    {
        perror("start incoming clock: unable to obtain key: ftok(3) failed");
        return 1;
    }

    // Get ID of the shared memory segment
    int shmid = shmget(key, 0, 0);
    if (errno)
    {
        perror("start incoming clock: unable to get shm: shmget(2) failed");
        return 2;
    }

    // Attach shared memory segment as read-only
    void* shm = shmat(shmid, NULL, SHM_RDONLY);
    if (errno)
    {
        perror("start incoming clock: unable to attach shm: shmat(2) failed");
        return 3;
    }

    self->running = CLOCK_RUNNING;
    self->shmid = shmid;
    self->__mem = shm;

    return -2;
}

/**
 * Start the clock under OUT mode.
 */
static int clock_start_out(clock_s* self)
{
    if (self->running)
        return -100;

    errno = 0;

    // Obtain the IPC key
    key_t key = ftok(FTOK_PATH, FTOK_CHAR);
    if (errno)
    {
        perror("start outgoing clock: unable to obtain key: ftok(3) failed");
        return 1;
    }

    // Create a shared memory segment
    int shmid = shmget(key, sizeof(__clock_mem_s), IPC_CREAT | IPC_EXCL | 0600);
    if (errno)
    {
        perror("start outgoing clock: unable to create shm: shmget(2) failed");
        return 2;
    }

    // Attach shared memory segment
    void* shm = shmat(shmid, NULL, 0);
    if (errno)
    {
        perror("start outgoing clock: unable to attach shm: shmat(2) failed");

        // Destroy segment
        shmctl(shmid, IPC_RMID, NULL);
        if (errno)
        {
            perror("start outgoing clock: unable to remove shm: shmctl(2) failed");
            return 4;
        }

        return 3;
    }

    self->running = CLOCK_RUNNING;
    self->shmid = shmid;
    self->__mem = shm;

    return -2;
}

/**
 * Stop the clock under IN mode.
 */
static int clock_stop_in(clock_s* self)
{
    if (self->running)
        return -100;

    errno = 0;

    // Detach shared memory segment
    shmdt(self->__mem);
    if (errno)
    {
        perror("stop incoming clock: unable to detach shm: shmdt(2) failed");
        return 1;
    }

    self->running = CLOCK_NOT_RUNNING;
    self->shmid = 0;
    self->__mem = NULL;

    return -2;
}

/**
 * Stop the clock under OUT mode.
 */
static int clock_stop_out(clock_s* self)
{
    if (!self->running)
        return -100;

    errno = 0;

    // Detach shared memory segment
    shmdt(self->__mem);
    if (errno)
    {
        perror("stop outgoing clock: unable to detach shm: shmdt(2) failed");
        return 1;
    }

    shmctl(self->shmid, IPC_RMID, NULL);
    if (errno)
    {
        perror("stop outgoing clock: unable to remove shm: shmctl(2) failed");
        return 2;
    }

    self->running = CLOCK_NOT_RUNNING;
    self->shmid = 0;
    self->__mem = NULL;

    return -2;
}

clock_s* clock_construct(clock_s* self, int mode)
{
    self->mode = mode;
    self->running = CLOCK_NOT_RUNNING;
    self->shmid = -1;
    self->__mem = NULL;

    switch (self->mode)
    {
    case CLOCK_MODE_IN:
        clock_start_in(self);
        break;
    case CLOCK_MODE_OUT:
        clock_start_out(self);
        break;
    default:
        break;
    }

    return self;
}

clock_s* clock_destruct(clock_s* self)
{
    // Stop clock if running
    if (self->running)
    {
        switch (self->mode)
        {
        case CLOCK_MODE_IN:
            clock_stop_in(self);
            break;
        case CLOCK_MODE_OUT:
            clock_stop_out(self);
            break;
        default:
            break;
        }
    }

    return self;
}

void clock_tick(clock_s* self)
{
    int nanos = self->__mem->nanos;
    int seconds = self->__mem->seconds;

    // In this simulation, we increment by 8 nanoseconds per tick
    // Overflow into seconds once maximum fractional second is reached
    nanos += 8;
    if (nanos >= 1000000000)
    {
        seconds += nanos / 1000000000;
        nanos %= 1000000000;
    }

    self->__mem->nanos = nanos;
    self->__mem->seconds = seconds;
}

int clock_get_nanos(clock_s* self)
{
    return self->__mem->nanos;
}

int clock_get_seconds(clock_s* self)
{
    return self->__mem->seconds;
}
