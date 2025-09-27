#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "math-utils.h"
#include "signal.h"

// prototypes
static void restore_terminal_settings(void);
static void init_non_block_input(void);
static void init_idt_table(void);
static void init_signals_table(void);

static void find_control_char(unsigned char command, char* result);
static void find_command(char (*command)[15]);
static void find_command_by_symbol(const char symbol);
static _Bool signal_is_enabled(const char(*name));
static int multiple_signal_choose(void);

/*
 * user functions
 */
static void list_commands(void);
static void trigger_signal(void);
static void quit_program(void);
static void mask_signal(void);
static void unmask_signal(void);
static void clear_buffer(void);

static void generic_error_handler(char* message);

/*
 * associate signals to keys
 */
typedef struct {
    char name[10];
    unsigned char key;
    _Bool enabled;
} SIGNALS;

SIGNALS signals[5];

/*
 * struct to define handlers
 */
typedef struct {
    char name[10];
    char description[50];
    char keymaps[3];
    void (*f)(void);
} IDT;

// initialize an array of structs
IDT idt[10];

static struct termios old_tio, new_tio;

/*
 * struct to group some utils variables
 */
typedef struct {
    _Bool quit;
    char buffer[3];
} UTILS;

UTILS utils = {0};

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
    init_signals_table();

    printf("IDT simulator\n\n");
    printf("  Type ':' to enter in command line\n");
    printf(
        "  Use the 'listc' (in command line) command or the '^L' symbol to "
        "see all available "
        "commands\n\n");

    char c;
    do {
        if (utils.quit) {
            restore_terminal_settings();
            printf("quit: Program interrupted by user\n");
            break;
        }

        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 0x3A) /* hexadecimal value for : */ {
                /*
                 * store command entered by user
                 */
                char command[15];
                clear_buffer();
                int limit = (int)(sizeof(command) / sizeof(command[0]));
                for (int i = 0; i <= limit; i++) {
                    if (read(STDIN_FILENO, &c, 1) > 0) {
                        if (i == limit) {
                            command[i] = '\0';
                        }

                        if (c == 0x0A) /* hexadecimal value for return */ {
                            if (i == 0) {
                                continue;
                            }
                            command[i] = '\0';
                            break;
                        } else if (c == 0x08 && i != 0) /* hexadecimal value for backspace */ {
                            command[i - 1] = '\0';
                            i--;
                        } else {
                            command[i] = c;
                        }
                    } else {
                        perror("\nKey not read\n");
                    }
                }

                /*
                 * calls `find_command` function to try to find the entered command
                 */
                find_command(&command);
                strcpy(command, "");
            } else if (c == 0x0A) /* hexadecimal value for return key */ {
                continue;

                /*
                 * for other cases, try to find a symbol
                 */
            } else {
                /*
                 * this block will treat symbols and associate them to commands using
                 * the specific function 'find_command_by_symbol'
                 */
                clear_buffer();
                find_command_by_symbol(c);
            }
        } else {
            perror("\nThe entered key could not be read\n");
        }
    } while (1);  // infinite loop (will be break only with 'q' or '^C' interrupt
                  // keymaps)
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
    strcpy(idt[0].name, "listc\0");
    strcpy(idt[0].description, "Lists all available commands");
    idt[0].keymaps[0] = 0x0C;  // ^L
    idt[0].f = list_commands;

    strcpy(idt[1].name, "trigger\0");
    strcpy(idt[1].description, "Triggers a signal");
    idt[1].keymaps[0] = 0x14;  // ^T
    idt[1].f = trigger_signal;

    strcpy(idt[2].name, "quit\0");
    strcpy(idt[2].description, "Shutdown the program");
    idt[2].keymaps[0] = 0x71;  // q
    idt[2].keymaps[1] = 0x51;  // Q
    idt[2].f = quit_program;

    strcpy(idt[3].name, "mask\0");
    strcpy(idt[3].description, "Masks an enabled signal");
    idt[3].keymaps[0] = 0x0D;  // ^M
    idt[3].f = mask_signal;

    strcpy(idt[4].name, "unmask\0");
    strcpy(idt[4].description, "Unmasks a disabled signal");
    idt[4].keymaps[0] = 0x15;  // ^U
    idt[4].f = unmask_signal;

    strcpy(idt[5].name, "clear\0");
    strcpy(idt[5].description, "Clear the current buffer");
    idt[5].keymaps[0] = 0x43;  // C
    idt[5].f = clear_buffer;
}

/*
 * map signals to keys
 */
void init_signals_table(void) {
    strcpy(signals[0].name, "SIGINT\0");
    signals[0].key = 0x03;
    signals[0].enabled = 1;

    strcpy(signals[1].name, "SIGQUIT\0");
    signals[1].key = 0x71;
    signals[1].enabled = 1;
}

/*
 * function to find and call a function by the given command
 */
static void find_command(char (*command)[15]) {
    for (int i = 0; i < (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        if (strcmp(idt[i].name, *command) == 0) {
            idt[i].f();
            return;
        }
    }
    printf("\nCommand '%s' not found\n", *command);
}

/*
 * function to find command by the given symbol
 */
static void find_command_by_symbol(const char symbol) {
    /*
    if (!signal_is_enabled(&symbol)) {
        find_control_char(symbol, utils.buffer);
        printf("The signal associated to '%s' is current disabled\n", utils.buffer);
        return;
    }

    change functions names to add more clearity
    */

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
        for (int j = 0; j < (int)(sizeof(idt[i].keymaps) / sizeof(idt[i].keymaps[0])); j++) {
            if (idt[i].keymaps[j] == symbol) {
                idt[i].f();
                return;
            }
        }
    }
    find_control_char(symbol, utils.buffer);
    printf("Symbol '%s' not mapped\n", utils.buffer);
}

/*
 * function to trigger a signal
 */
static void trigger_signal(void) {
    int index = multiple_signal_choose();
    if (index == -1) {
        return;
    }

    if (!signal_is_enabled((const char*)&signals[index - 1].name)) {
        printf("\n%s is current disabled\n", signals[index - 1].name);
        return;
    }

    find_command_by_symbol((const char)signals[index - 1].key);
}

/*
 * a function to mask a signal and disable it
 */
static void mask_signal(void) {
    int index = multiple_signal_choose();
    if (index == -1) {
        return;
    }

    if (!signal_is_enabled((const char*)&signals[index - 1].name)) {
        printf("\n%s signals is already disabled\n", signals[index - 1].name);
        return;
    }

    signals[index - 1].enabled = 0;
    printf("\n%s signal has been disabled\n", signals[index - 1].name);
}

static void unmask_signal(void) {
    int index = multiple_signal_choose();
    if (index == -1) {
        return;
    }

    if (signal_is_enabled((const char*)&signals[index - 1].name)) {
        printf("\n%s signals is already active\n", signals[index - 1].name);
        return;
    }

    signals[index - 1].enabled = 1;
    printf("\n%s signal has been active\n", signals[index - 1].name);
}

/*
 * util function to show all available signals and allow to choose one of it
 */
static int multiple_signal_choose(void) {
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        if (strcmp((const char*)signals[i].name, "") != 0) {
            printf("%s: %d\n", signals[i].name, i + 1);
        }
    }

    char c;
    char number[4];
    int index = 0;

    printf("\nEnter the index of the signal you want to trigger:\n");
    do {
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == 0x0A || index >= (int)(sizeof(number) / sizeof(number[0])) - 1) {
                break;
            }
            number[index] = c;
            index++;
        } else {
            perror("The key could not be read\n");
        }
    } while (1);
    number[index] = '\0';

    /*
     * uses a local function to convert the string to int
     */
    index = convert_from_string_to_int(number);

    if (index <= 0 || index > (int)(sizeof(signals) / sizeof(signals[0]))) {
        printf("\nSignal not available for the index '%d'\n", index);
        return -1;
    }
    return index;
}

/*
 * function to verify if a signal is current enabled using its name
 */
static _Bool signal_is_enabled(const char(*name)) {
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        if (strcmp((const char*)signals[i].name, name) == 0) {
            return (signals[i].enabled) ? 1 : 0;
        }
    }
    return 1;
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
            printf(" %s: %s\n", idt[i].name, idt[i].description);
        }
    }
}

/*
 * function to format control characters in a better way
 */
static void find_control_char(unsigned char command, char* result) {
    /*
     * verifies if it is a control character
     */
    if (iscntrl(command) && command < 32) {
        result[0] = '^';
        result[1] = command + 64;
        result[2] = '\0';
    } else if (command == 127) {
        result[0] = '^';
        result[1] = '?';
        result[2] = '\0';
    } else {
        result[0] = command;
        result[1] = '\0';
    }
}

static void quit_program(void) { utils.quit = 1; }

/*
 * local function to clear the buffer
 */
static void clear_buffer(void) {
    /*
     * calls the system command to clear the buffer
     */
    system("clear");
}

/*
 * simple function to simulate errors handlers
 */
static void generic_error_handler(char* message) { printf("\n%s", message); }
