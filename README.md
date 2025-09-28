# IDT simulator

This is a simple IDT Linux-based simulator.

---

### What is IDT?

In x86 architeture-based systems, the **Interrupt Descriptor Table** is a table that the processor verifies if an external event occurs.

E.G.:

- Exceptions
- Hardware interruptions
- Software interruptions

## You can learning more by reading this [article](https://wiki.osdev.org/Interrupt_Descriptor_Table).

---

### Core functionalities

- Non-block terminal (do not expects for return key to register command)
- Maps its own signals table
- Support to a mini shell with custome commands, easily defined
- Many keys can be associate to a single command
- Logs support

---

### Non-block input

This project uses the `Termios` struct to represents terminal settings:

```c
struct termios {
   tcflag_t c_iflag;  // input flags
   tcflag_t c_oflag;  // output flags
   tcflag_t c_cflag;  // control flags
   tcflag_t c_lflag;  // local flags
   cc_t c_cc[NCCS];   // control characters
   // ...
}
```

It creates two variables of this type:

- `old_tio`: to store the old terminal input/output settings to be restore later
- `new_tio`: `old_tio` with modifications

---

### Main user functions

User functions are defined in here:

```c
static void init_idt_table(void) {
    strcpy(idt[0].name, "listc\0");
    strcpy(idt[0].description, "Lists all available commands");
    idt[0].keymaps[0] = 0x0C;  // ^L
    idt[0].f = list_commands;

    // more
}
```

`idt` is an array of structs, which defines some useful members:

```c
typedef struct {
    char name[15]; // command name
    char description[50]; // command description
    char keymaps[3]; // keymaps to trigger the command
    void (*f)(void); // pointer to function that will be triggered
} IDT;

IDT idt[10]; // initialize an array of structs with 10 spaces (can be increased)
```

You can use any of this commands in command line mode (`:`) by typing their names or enter their keys in normal mode

---

### Dependencies

[![C](https://img.shields.io/badge/C-00599C?logo=c&logoColor=white)](#)<br>
[![GNU](https://img.shields.io/badge/GNU-000000?logo=gnu&logoColor=white)](#)

- GCC
- GDB

- Make

---

### How to run it?

Build:

```bash
make all
```

Run:

```bash
make run
```

Debug:

```bash
make debug
```

Clean:

```bash
make clean
```

---

Made with  using  and 
