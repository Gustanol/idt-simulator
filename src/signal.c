#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
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
static int multiple_signal_choose(const char* name);
static void register_log(const char* command);

/*
 * user functions
 */
static void list_commands(void);
static void trigger_signal(void);
static void quit_program(void);
static void mask_signal(void);
static void unmask_signal(void);
static void mask_all_signals(void);
static void unmask_all_signals(void);
static void clear_buffer(void);
static void print_logs(void);

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
    char name[15];
    char description[50];
    char keymaps[3];
    void (*f)(void);
} IDT;

// initialize an array of structs
IDT idt[10];

static struct termios old_tio, new_tio;

/*
 * struct to group some useful variables
 */
typedef struct {
    _Bool quit;
    char buffer[3];
    const char current_signal;
    int log_index;
} UTILS;

UTILS utils = {0};

/*
 * struct to store logs of the current session
 */
typedef struct {
    time_t timestamp;
    struct tm tm_info;
    char command[50];
} LOGS;

LOGS logs[30];

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
            printf("INTERRUPTION: Program interrupted by user\n");
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

                register_log(command);

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
                register_log(&c);
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
    new_tio.c_lflag &= ~(ICANON | ISIG);
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
     * defines 'listc' as a command
     *
     * it prints all available mappend commands
     */
    strcpy(idt[0].name, "listc\0");
    strcpy(idt[0].description, "Lists all available commands");
    idt[0].keymaps[0] = 0x0C;  // ^L
    idt[0].f = list_commands;

    /*
     * defines 'trigger' as a command
     *
     * it triggers a specific defined signal
     */
    strcpy(idt[1].name, "trigger\0");
    strcpy(idt[1].description, "Triggers a signal");
    idt[1].keymaps[0] = 0x14;  // ^T
    idt[1].f = trigger_signal;

    /*
     * defines 'quit' as a command
     *
     * it just shutdown the program
     */
    strcpy(idt[2].name, "quit\0");
    strcpy(idt[2].description, "Interrupts the program");
    idt[2].keymaps[0] = 0x71;  // q
    idt[2].keymaps[1] = 0x51;  // Q
    idt[2].keymaps[2] = 0x03;  // ^C
    idt[2].f = quit_program;

    /*
     * defines 'mask' as a command
     *
     * it masks (disable) a signal
     */
    strcpy(idt[3].name, "mask\0");
    strcpy(idt[3].description, "Masks an enabled signal");
    idt[3].keymaps[0] = 0x0D;  // ^M
    idt[3].f = mask_signal;

    /*
     * defines 'unmask' as a command
     *
     * it unmasks (enable) a signal
     *
     * it has the opposite function of 'mask' command
     */
    strcpy(idt[4].name, "unmask\0");
    strcpy(idt[4].description, "Unmasks a disabled signal");
    idt[4].keymaps[0] = 0x15;  // ^U
    idt[4].f = unmask_signal;

    /*
     * defines 'maskall' as a command
     *
     * it masks all enabled signals
     */
    strcpy(idt[5].name, "maskall\0");
    strcpy(idt[5].description, "Masks all signals");
    idt[5].keymaps[0] = 0x0B;  // ^K
    idt[5].f = mask_all_signals;

    /*
     * defines 'unmaskall' as a command
     *
     * it unmasks all disabled signals
     */
    strcpy(idt[6].name, "unmaskall\0");
    strcpy(idt[6].description, "Unmasks all signals");
    idt[6].keymaps[0] = 0x01;  // ^A
    idt[6].f = unmask_all_signals;

    /*
     * defines 'clear' as a local command
     *
     * it clear the buffer
     */
    strcpy(idt[7].name, "clear\0");
    strcpy(idt[7].description, "Clear the current buffer");
    idt[7].keymaps[0] = 0x43;  // C
    idt[7].f = clear_buffer;

    /*
     * defines 'logs' as a command
     *
     * it prints some logs of the current session (not all)
     */
    strcpy(idt[8].name, "logs\0");
    strcpy(idt[8].description, "Print some logs of the current session");
    idt[8].keymaps[0] = 0x4C;
    idt[8].keymaps[1] = 0x6C;
    idt[8].f = print_logs;
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

    strcpy(signals[2].name, "SIGLIST\0");
    signals[2].key = 0x0C;
    signals[2].enabled = 1;

    strcpy(signals[3].name, "SIGTRI\0");
    signals[3].key = 0x14;
    signals[3].enabled = 1;
}

/*
 * function to find and call a function by the given command
 */
static void find_command(char (*command)[15]) {
    for (int i = 0; i < (int)(sizeof(idt) / sizeof(idt[0])); i++) {
        if (strcmp(idt[i].name, *command) == 0) {
            if (!signal_is_enabled(&idt[i].keymaps[0])) {
                printf("The signal associated to '%s' is current disabled\n", *command);
                return;
            }
            idt[i].f();
            return;
        }
    }
    printf("\n  Command '%s' not found\n", *command);
}

/*
 * function to find command by the given symbol
 */
static void find_command_by_symbol(const char symbol) {
    if (!signal_is_enabled(&symbol)) {
        find_control_char(symbol, utils.buffer);
        printf("The signal associated to '%s' is current disabled\n", utils.buffer);
        return;
    }

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
    printf("  Symbol '%s' not mapped\n", utils.buffer);
}

/*
 * function to trigger a signal
 */
static void trigger_signal(void) {
    int index = multiple_signal_choose("trigger");
    if (index == -1) {
        return;
    }

    if (!signal_is_enabled((const char*)&signals[index - 1].name)) {
        printf("\n  %s is current disabled\n", signals[index - 1].name);
        return;
    }

    find_command_by_symbol((const char)signals[index - 1].key);
}

/*
 * a function to mask a signal (disable it)
 */
static void mask_signal(void) {
    int index = multiple_signal_choose("mask");
    if (index == -1) {
        return;
    }

    if (!signal_is_enabled((const char*)&signals[index - 1].name)) {
        printf("\n  %s signals is already disabled\n", signals[index - 1].name);
        return;
    }

    signals[index - 1].enabled = 0;
    printf("\n  %s signal has been disabled\n", signals[index - 1].name);
}

/*
 * function to masks all active signals at once
 */
static void mask_all_signals(void) {
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        if (signals[i].enabled) {
            signals[i].enabled = 0;
        }
    }
    printf("  All signals were masked\n");
}

/*
 * function to unmasks all disabled signals at once
 */
static void unmask_all_signals(void) {
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        if (!signals[i].enabled) {
            signals[i].enabled = 1;
        }
    }
    printf("  All signals were unmasked\n");
}

/*
 * function to unmask a given signal
 */
static void unmask_signal(void) {
    int index = multiple_signal_choose("unmask");
    if (index == -1) {
        return;
    }

    if (signal_is_enabled((const char*)&signals[index - 1].name)) {
        printf("\n  %s signals is already active\n", signals[index - 1].name);
        return;
    }

    signals[index - 1].enabled = 1;
    printf("\n  %s signal has been active\n", signals[index - 1].name);
}

/*
 * util function to show all available signals and allow to choose one of it
 */
static int multiple_signal_choose(const char* name) {
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        if (strcmp((const char*)signals[i].name, "") != 0) {
            printf("%s: %d\n", signals[i].name, i + 1);
        }
    }

    char c;
    char number[4];
    int index = 0;

    printf("\nEnter the index of the signal you want to %s:\n", name);
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
    _Bool active = 1;
    for (int i = 0; i < (int)(sizeof(signals) / sizeof(signals[0])); i++) {
        if (signals[i].key == (unsigned char)*name) {
            active = (signals[i].enabled) ? 1 : 0;
        }
    }
    return active;
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
            printf(" %s: %s [", idt[i].name, idt[i].description);
            for (int j = 0; j < (int)(sizeof(idt[i].keymaps) / sizeof(idt[i].keymaps[j])); j++) {
                if (idt[i].keymaps[j] != *"") {
                    find_control_char(idt[i].keymaps[j], utils.buffer);
                    printf(" %s", utils.buffer);
                }
            }
            printf(" ]\n");
        }
    }
}

/*
 * function to format control characters in a better way
 */
static void find_control_char(unsigned char command, char* result) {
    strcpy(result, "");
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

/*
 * a function to register new logs
 */
static void register_log(const char* command) {
    // just returns if the current index of the array is greater or equal than 30
    if (utils.log_index >= 30) return;

    strcpy(logs[utils.log_index].command, command);
    logs[utils.log_index].command[sizeof(logs[utils.log_index].command) - 1] = '\0';

    /*
     * obtains the timestamp (not too readable)
     */
    logs[utils.log_index].timestamp = time(NULL);

    /*
     * store in the current index of 'logs' array using 'tm' struct, provide by 'time.h'
     */
    logs[utils.log_index].tm_info = *localtime(&logs[utils.log_index].timestamp);

    utils.log_index++;
}

/*
 * user function to print all stored logs
 */
static void print_logs(void) {
    for (int i = 0; i < (int)(sizeof(logs) / sizeof(logs[0])); i++) {
        if (strcmp(logs[i].command, "") != 0) {
            char time_str[20];

            /*
             * formats the timestamp
             */
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &logs[i].tm_info);
            printf("[%s]: %s\n", time_str, logs[i].command);
        }
    }
}
