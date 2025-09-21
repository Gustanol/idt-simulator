#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "signal.h"

// prototypes
static void init_non_block_input(void);
static void init_idt_table(void);
static void restore_terminal_settings(void);
static void find_command(char (*command)[10]);
static void find_command_by_symbol(const char* symbol);
static void list_commands(void);

/*
 * struct to define handlers
 */
typedef struct {
    char name[10];
    char description[50];
    char (*keymaps)[3];
    void (*f)(void);
} IDT;

// initialize an array of structs
IDT idt[10];

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

void run_program(void) {
    /*
     * call function to init non-block terminal input
     */
    init_non_block_input();
    init_idt_table();

    printf("IDT simulator\n");
    printf(
        "Type ':' to enter in command line and enter 'listc' command to list "
        "all available "
        "commands\n\n");

    char c;
    do {
        if (read(STDIN_FILENO, &c, 1) > 0) {
            printf("%c", c);
            if (c == 'q') {
                restore_terminal_settings();
                break;
            } else if (c == 0x3A) /* hexadecimal value for : */ {
                /*
                 * store command entered by user
                 */
                char command[10];
                system("clear");
                for (int i = 0; i < (int)sizeof(command); i++) {
                    char c = getchar();
                    if (c == 0x0A) {
                        break;
                    }
                    command[i] = c;
                }

                /*
                 * calls `find_command` function to try to find the entered command
                 */
                find_command(&command);
                strcpy(command, "");
            } else {
                /*
                 * this block will treat symbols and associate them to commands using the specific
                 * function 'find_command_by_symbol'
                 */
                find_command_by_symbol(&c);
            }
        }
    } while (1);
}

static void init_non_block_input(void) {
    /*
     * read all current terminal settings and store it into old_tio (terminal
     * input/output)
     */
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;

    /*
     * disable canonical mode - buffered input (expects no longer for enter key to
     * register commands)
     */
    new_tio.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);  // apply settings
}

static void restore_terminal_settings(void) {
    /*
     * restore terminal settings using data stored in old_tio struct
     */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
}

static void init_idt_table(void) {
    /*
     * define 'listc' as a command
     */
    strcpy(idt[0].name, "listc");
    strcpy(idt[0].description, "Lists all available commands");
    *idt[0].keymaps[0] = 0x0C;
    idt[0].f = list_commands;
}

/*
 * function to find and call a function by the given command
 */
static void find_command(char (*command)[10]) {
    for (int i = 0; i < (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        if (strcmp(idt[i].name, *command) == 0) {
            idt[i].f();
            return;
        }
    }
    printf("Command '%s' not found", *command);
}

/*
 * function to find command by the given symbol
 */
static void find_command_by_symbol(const char* symbol) {
    /*
     * iterates for each command
     *
     * in it, iterates for each keymap created for the command
     *
     * verifies if the given symbol corresponds to the current keymap
     *
     * if so, call the function associated to it
     */
    for (int i = 0; i < (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        for (int j = 0; j < (int)(sizeof(*idt[i].keymaps) / sizeof(*idt[i].keymaps[0])); j++) {
            if (strcmp(idt[i].keymaps[j], symbol) == 0) {
                idt[i].f();
                return;
            }
        }
    }
    printf("Symbol '%c' not mapped", *symbol);
}

/*
 * function to list all available commands
 */
static void list_commands(void) {
    /*
     * it iterates from zero to idt array size
     *
     * P.S.: sizeof function returns the size in bytes. That is why we must divide
     * it to the size, bytes, of the first (or another one) element
     */
    for (int i = 0; i < (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        if (strcmp(idt[i].name, "") != 0) {
            printf("- %s: %s\n", idt[i].name, idt[i].description);
        }
    }
}
