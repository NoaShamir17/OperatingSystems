// signals.c
#include "signals.h"
#include "signals.h"

// Triggered by Ctrl+Z (SIGTSTP)
void ctrlZHandler(int sig_num) {
    // TODO: Implement logic to catch SIGTSTP and stop foreground process
}

// Triggered by Ctrl+C (SIGINT)
void ctrlCHandler(int sig_num) {
    // TODO: Implement logic to catch SIGINT and kill foreground process
}