/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <getopt.h>
#include <errno.h>

#define LOG_TAG "ttyfwd"
#include <utils/Log.h>

/* structure to hold the tty device data */
struct ttydev
{
    char *name;
};

/* structure to hold the thread context, one per thread */
#define BUFFER_SIZE 16*1024
struct thrctx
{
    char            buff[BUFFER_SIZE];
    struct ttydev   *src;
    struct ttydev   *dst;
    int             index;
};

/* structure to hold all global data */
struct ttyfwd
{
    int             timeout;
    struct ttydev   ttydev[2];
    struct thrctx   threadctx[2];
    pthread_cond_t  cond_halt;
    pthread_mutex_t mutex_halt;
    int             abort;
};

struct ttyfwd myttyfwd =
{
    .timeout    = 30,
    .cond_halt  = PTHREAD_COND_INITIALIZER,
    .mutex_halt = PTHREAD_MUTEX_INITIALIZER,
    .abort      = 0,
};

static struct ttyfwd *p = &myttyfwd;


static void usage(void)
{
    ALOGI("ttyfwd -d </dev/ttydev1> -d </dev/ttydev2> [-t <timeout (sec)>]");
}

static int configure_tty(int fd, char* dev)
{
    struct termios ios;
    int ret;

    ret = tcgetattr(fd, &ios);
    if (ret)
    {
        ALOGW("Error in tcgetaddr(%s) for %s", strerror(errno), dev);
        return ret;
    }
    ios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    ios.c_oflag &= ~(OPOST);
    ios.c_cflag &= ~(CSIZE | PARENB);
    ios.c_cflag |= (CS8 | CLOCAL | CREAD);
    ios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    ios.c_cc[VTIME] = 0;
    ios.c_cc[VMIN] = 1;
    ret = tcsetattr(fd, TCSANOW, &ios);
    if (ret)
        ALOGW("Error in tcsetattr(%s) for %s", strerror(errno), dev);
    return ret;
}

static void *forward_thread (void *arg)
{
    struct thrctx *threadctx = (struct thrctx *) arg;
    int fds, fdd, countdown, ret;
    sigset_t sset;

    ALOGI("[%d] Starting forward thread: %s -> %s",
          threadctx->index, threadctx->src->name, threadctx->dst->name);

    /* Enable USR1 signal */
    sigemptyset(&sset);
    sigaddset(&sset, SIGUSR1);
    ret = pthread_sigmask(SIG_UNBLOCK, &sset, NULL);
    if (ret)
    {
        ALOGE("[%d] pthread_sigmask error (%d)",
                  threadctx->index, ret);
        goto exit;
    }

    /* Open the source TTY - Read-Only */
    countdown = p->timeout;
    while((countdown > 0) && (!p->abort))
    {
        fds = open(threadctx->src->name, O_RDONLY);
        if (fds < 0)
        {
            countdown--;
            sleep(1);
            ALOGD("[%d] Open %s failed (%s) - retry count %d",
                  threadctx->index, threadctx->src->name, strerror(errno), countdown);
        }
        else
            break;
    }
    if (fds < 0)
        goto exit;

    /* Configure the source TTY */
    ret = configure_tty(fds, threadctx->src->name);
    if (ret)
        goto exit;

    /* Open the destination TTY - Write-Only */
    countdown = p->timeout;
    while((countdown > 0) && (!p->abort))
    {
        fdd = open(threadctx->dst->name, O_WRONLY);
        if (fdd < 0)
        {
            countdown--;
            sleep(1);
            ALOGD("[%d] Open %s failed (%s) - retry count %d",
                  threadctx->index, threadctx->dst->name, strerror(errno), countdown);
        }
        else
            break;
    }
    if (fdd < 0)
        goto exit;

    /* Configure the destination TTY */
    ret = configure_tty(fdd, threadctx->dst->name);
    if (ret)
        goto exit;

    /* Forward loop */
    while (!p->abort)
    {
        int size, tocopy;
        char *buff;

        buff = threadctx->buff;
        size = read(fds, buff, BUFFER_SIZE);
        if (size <= 0)
        {
            if (size == 0)
                ALOGE("[%d] Read EOF", threadctx->index);
            else
                ALOGE("[%d] Read error (%s)", threadctx->index , strerror(errno));
            goto exit;
        }

        tocopy = size;
        while ((tocopy > 0) && (!p->abort))
        {
            size = write(fdd, buff, tocopy);
            if (size == 0)
                ALOGW("[%d] Write null", threadctx->index);
            if (size < 0)
            {
                ALOGE("[%d] Write error (%s)", threadctx->index , strerror(errno));
                goto exit;
            }
            tocopy -= size;
            buff += size;
        }
    }

exit:
    ALOGI("[%d] Thread terminating", threadctx->index);

    if (fds >= 0)
        close(fds);
    if (fdd >= 0)
        close(fdd);

    /* Tell main thread about termination */
    pthread_mutex_lock (&(p->mutex_halt));
    p->abort = 1;
    pthread_cond_signal(&(p->cond_halt));
    pthread_mutex_unlock (&(p->mutex_halt));

    pthread_exit(NULL);
    return NULL;
}


static void usr1_sig_handler(int signum)
{
    ALOGD("SIGUSR1 received");
    p->abort = 1;
}


int main(int argc, char **argv)
{
    int c, ret, ttydev_num = 0;
    pthread_t thread0, thread1;
    void *pret;
    sigset_t sset;
    struct sigaction saction;

    while ((c = getopt(argc, argv, "d:t:h?")) != -1)
    {
        switch(c)
        {
            case 'd':
                if (ttydev_num < 2)
                {
                    p->ttydev[ttydev_num].name = optarg;
                    ttydev_num++;
                }
                break;
            case 't':
                p->timeout = atoi(optarg);
                break;
            case 'h':
            case '?':
                usage();
                exit(EXIT_FAILURE);
            default:
                ALOGE("unknown command flag (%c)\n", c);
                exit(EXIT_FAILURE);
        }
    }

    if (ttydev_num != 2)
    {
        ALOGE("Missing device definition");
        usage();
        exit(EXIT_FAILURE);
    }

    ALOGI("Forward %s <-> %s - timeout: %d", p->ttydev[0].name, p->ttydev[1].name, p->timeout);

    /* Startup the forwarding threads */
    p->threadctx[0].src = &p->ttydev[0];
    p->threadctx[0].dst = &p->ttydev[1];
    p->threadctx[0].index = 0;
    p->threadctx[1].src = &p->ttydev[1];
    p->threadctx[1].dst = &p->ttydev[0];
    p->threadctx[1].index = 1;

    /* Mask SIGUSR1 signal in main thread */
    sigemptyset(&sset);
    sigaddset(&sset, SIGUSR1);
    ret = pthread_sigmask(SIG_BLOCK, &sset, NULL);
    if (ret)
    {
        ALOGE("pthread_sigmask error (%d)", ret);
        exit(EXIT_FAILURE);
    }

    /* Register SIGUSR1 signal handler for threads */
    saction.sa_flags = 0;
    saction.sa_handler = usr1_sig_handler;
    sigemptyset(&saction.sa_mask);
    ret = sigaction(SIGUSR1, &saction, NULL);
    if (ret)
    {
        ALOGE("sigaction error (%d)", ret);
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&thread0, NULL, forward_thread, &p->threadctx[0]);
    if (ret)
    {
        ALOGE("Thread #0 creation error: %d", ret);
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&thread1, NULL, forward_thread, &p->threadctx[1]);
    if (ret)
    {
        ALOGE("Thread #1 creation error: %d", ret);
        exit(EXIT_FAILURE);
    }

    /* Wait for one thread to exit - terminate all */
    pthread_mutex_lock (&(p->mutex_halt));
    while (!p->abort)
    {
        ALOGD("Wait for one thread to exit");
        pthread_cond_wait (&(p->cond_halt), &(p->mutex_halt));
    }
    pthread_mutex_unlock (&(p->mutex_halt));
    ALOGI("One thread terminated - sending signal to unblock other");
    pthread_kill(thread0, SIGUSR1);
    pthread_kill(thread1, SIGUSR1);

    /* Wait for threads completion */
    ret = pthread_join(thread0, &pret);
    if (ret)
        ALOGD("Could not join thread0 (%d)", ret);
    ret = pthread_join(thread1, &pret);
    if (ret)
        ALOGD("Could not join thread0 (%d)", ret);

    ALOGI("All threads exited");

    exit (EXIT_SUCCESS);
}
