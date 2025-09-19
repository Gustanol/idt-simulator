#include <pthread.h>
#include <termios.h>
#include <unistd.h>

#include "signal.h"

static struct termios old_tio, new_tio;

void init_non_block_input(void) {
    /*
     * read all current terminal settings and store it into old_tio (terminal input/output)
     */
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;

    /*
     * disable canonical mode - buffered input (expects no longer for enter key to register
     * commands)
     */
    new_tio.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);  // apply settings
}

void restore_terminal_settings(void) {
    /*
     * restore terminal settings using data stored in old_tio struct
     */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

void run_program(void) {
    /*
     * call function to init non-block terminal input
     */
    init_non_block_input();
}
