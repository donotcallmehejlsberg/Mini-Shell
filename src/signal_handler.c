#include "signal_handler.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static volatile sig_atomic_t received_signal = 0;

static void handleSignal(int signal_number) { received_signal = signal_number; }

int setupSignalHandlers(void) {
  struct sigaction action = {0};

  action.sa_handler = handleSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  action.sa_handler = SIG_IGN;
  if (sigaction(SIGTSTP, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  // Let the shell reclaim the terminal while its process group is background
  if (sigaction(SIGTTOU, &action, NULL) == -1) {
    perror("sigaction SIGTTOU");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int restoreChildSignalHandlers(void) {
  struct sigaction action = {0};

  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  // External commands should use the normal terminal-output signal behavior
  if (sigaction(SIGTTOU, &action, NULL) == -1) {
    perror("sigaction SIGTTOU");
    return EXIT_FAILURE;
  }

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  if (sigaction(SIGTSTP, &action, NULL) == -1) {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

bool consumeInterruptSignal(void) {
  if (received_signal != SIGINT) {
    return false;
  }

  received_signal = 0;
  return true;
}
