// signals.c
#define _XOPEN_SOURCE 500
#include "signals.h"
#include "commands.h"
#include "my_system_call.h"
#include <signal.h>
#include <stdio.h>

extern Smash smash;

// Triggered by Ctrl+Z (SIGTSTP)
void ctrlZHandler(int sig_num) {
    // 1. Re-register the handler immediately to prevent it from resetting to default
    my_system_call(SYS_SIGNAL, SIGTSTP, ctrlZHandler); 

    printf("smash: caught CTRL+Z\n");

    // 2. If a foreground process exists (and is not smash itself)
    if (smash.fg_pid > 0) {
        // Send SIGSTOP to the foreground process
        my_system_call(SYS_KILL, smash.fg_pid, SIGSTOP);
        
        printf("smash: process %d was stopped\n", smash.fg_pid);
        
        // Mark pid as 0 so we don't try to stop it again
        smash.fg_pid = 0; 
    }
}

// Triggered by Ctrl+C (SIGINT)
void ctrlCHandler(int sig_num) {
    // 1. Re-register the handler immediately
    my_system_call(SYS_SIGNAL, SIGINT, ctrlCHandler);

    printf("smash: caught CTRL+C\n");

    if (smash.fg_pid > 0) {
        // Send SIGKILL to the foreground process
        my_system_call(SYS_KILL, smash.fg_pid, SIGKILL);
        
        printf("smash: process %d was killed\n", smash.fg_pid);
        
        smash.fg_pid = 0;
    } else {
        // Signal internal commands (like diff/quit) to stop
        smash.stop_internal_cmd = true;
    }
}