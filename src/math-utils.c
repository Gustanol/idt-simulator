#include "math-utils.h"

/*
 * simple function to convert string in int value
 */
int convert_from_string_to_int(const char* s) {
    int result = 0;
    int index = 0;

    while (s[index] != '\0') {
        result = result * 10 + (s[index++] - '0');
    }
    return result;
}
