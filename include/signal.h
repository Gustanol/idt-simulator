#ifndef SIGNAL_H
#define SIGNAL_H

/*
 * struct to define handlers
 */
typedef struct {
    int id;
    char name[10];
    void (*f)(void);
} IDT;

void run_program(void);

#endif /* SIGNAL_H */
