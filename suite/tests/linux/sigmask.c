/* **********************************************************
 * Copyright (c) 2016-2022 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * Test signal masks.
 */

#include "configure.h"
#ifndef UNIX
#    error UNIX-only
#endif
#include "tools.h"
#include "thread.h"
#include "condvar.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h> /* itimer */

static void *child_ready;

#define MAGIC_VALUE 0xdeadbeef

static void
handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    /* We go ahead and use locks which is unsafe in general code but we have controlled
     * timing of our signals here.
     */
    if (sig == SIGWINCH) {
#ifdef MACOS
        /* Make the output match for template simplicity. */
        siginfo->si_code = SI_QUEUE;
        siginfo->si_value.sival_ptr = (void *)MAGIC_VALUE;
#endif
        print("in handler for signal %d from %d value %p\n", sig, siginfo->si_code,
              siginfo->si_value);
    } else
        print("in handler for signal %d\n", sig);
    signal_cond_var(child_ready);
    if (sig == SIGUSR2)
        pthread_exit(NULL);
}

static void *
thread_routine(void *arg)
{
    intercept_signal(SIGUSR1, (handler_3_t)handler, false);
    intercept_signal(SIGWINCH, (handler_3_t)handler, false);
    intercept_signal(SIGUSR2, (handler_3_t)handler, false);

    signal_cond_var(child_ready);

    sigset_t set;
    sigemptyset(&set);
    while (true) {
        sigsuspend(&set);
    }
    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t thread;
    void *retval;

    child_ready = create_cond_var();

    if (pthread_create(&thread, NULL, thread_routine, NULL) != 0) {
        perror("failed to create thread");
        exit(1);
    }

    wait_cond_var(child_ready);
    /* Impossible to have child notify us when inside sigsuspend but it should get
     * there pretty quickly after it signals condvar.
     */
    reset_cond_var(child_ready);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGWINCH);
    int res = sigprocmask(SIG_BLOCK, &set, NULL);
    if (res != 0)
        perror("sigprocmask failed");

    /* Send a signal to the whole process.  This often is delivered to the
     * main (current) thread under DR where it's not blocked, causing a hang without
     * the rerouting of i#2311.
     */
    print("sending %d\n", SIGUSR1);
    kill(getpid(), SIGUSR1);
    wait_cond_var(child_ready);
    reset_cond_var(child_ready);

    print("sending %d with value\n", SIGWINCH);
    union sigval value;
    value.sival_ptr = (void *)MAGIC_VALUE;
#ifdef MACOS
    /* sigqueue is not available on Mac. */
    kill(getpid(), SIGWINCH);
#else
    sigqueue(getpid(), SIGWINCH, value);
#endif
    wait_cond_var(child_ready);
    reset_cond_var(child_ready);

    /* Tell thread to exit. */
    pthread_kill(thread, SIGUSR2);
    if (pthread_join(thread, &retval) != 0)
        perror("failed to join thread");

    destroy_cond_var(child_ready);

    print("all done\n");

    return 0;
}
