#ifndef SIGNAL_H
#define SIGNAL_H
#include <termios.h>

/*
 * struct to define handlers
 */
typedef struct {
    int id;
    char name[10];
    void (*f)(void);
} IDT;

static struct termios old_tio, new_tio;

/*
 * Termios struct
 struct termios {
    tcflag_t c_iflag;  // input flags
    tcflag_t c_oflag;  // output flags
    tcflag_t c_cflag;  // control flags
    tcflag_t c_lflag;  // local flags
    cc_t c_cc[NCCS];   // control characters
    ...
 }
 */

void run_program(void);
void init_non_block_input(void);
void restore_terminal_settings(void);

#endif /* SIGNAL_H */
