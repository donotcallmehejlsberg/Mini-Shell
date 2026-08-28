#ifndef MINI_SHELL_SIGNAL_HANDLER_H_
#define MINI_SHELL_SIGNAL_HANDLER_H_

#include <stdbool.h>

int setupSignalHandlers(void);
int restoreChildSignalHandlers(void);
bool consumeInterruptSignal(void);

#endif
