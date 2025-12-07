// signals.c
#define _XOPEN_SOURCE 500
#include "signals.h"
#include "commands.h"
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "my_system_call.h"

extern Smash smash;

void ctrlZHandler(int sig_num) {
    printf("smash: caught CTRL+Z\n"); //

    if (smash.fg_pid > 0) {
        // Send SIGSTOP to the foreground process
        my_system_call(SYS_KILL, smash.fg_pid, SIGSTOP);
        printf("smash: process %d was stopped\n", smash.fg_pid);
        
        // Note: We do NOT add to job list here. 
        // waitpid() in commands.c will detect the stop and handle it.
        smash.fg_pid = 0; 
    }
}

void ctrlCHandler(int sig_num) {
    printf("smash: caught CTRL+C\n"); //

    if (smash.fg_pid > 0) {
        // Send SIGKILL to the foreground process
        my_system_call(SYS_KILL, smash.fg_pid, SIGKILL);
        printf("smash: process %d was killed\n", smash.fg_pid);
        smash.fg_pid = 0;
    } else {
        // No external process? Signal internal commands (diff/quit) to stop.
        smash.stop_internal_cmd = true;
    }
}