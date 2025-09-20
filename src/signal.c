#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "signal.h"

void run_program(void) {
    /*
     * call function to init non-block terminal input
     */
    init_non_block_input();
    init_idt_table();

    char c;
    do {
        if (read(STDIN_FILENO, &c, 1)) {
            printf("%c", c);
            if (c == *"q") {
                break;
            } else if (c == 0x3A) /* hexadecimal value for : */ {
                restore_terminal_settings();

                /*
                 * store command entered by user
                 */
                char command[10];
                printf("\n:");
                scanf("%c", command);

                /*
                 * calls `find_command` function to try to find the entered command
                 */
                find_command(&command);
                init_non_block_input();
            }
        }
    } while (1);
}

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

void init_idt_table(void) {
    strcpy(idt[0].name, "listc");
    idt[0].f = list_commands;
}

/*
 * function to find and call a function by the given command
 */
void find_command(char (*command)[10]) {
    for (int i = 0; (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        if (idt[i].name == *command) {
            idt[i].f();
            return;
        }
    }
    printf("Command '%s' not found", *command);
}

/*
 * function to list all available commands
 */
void list_commands(void) {
    /*
     * it iterates from zero to idt array size
     *
     * P.S.: sizeof function returns the size in bytes. That is why we must divide it to the size,
     * bytes, of the first (or another one) element
     */
    for (int i = 0; i < (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        printf("Command %d: %s", i, idt[i].name);
    }
}
