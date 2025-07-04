#include "../include/util.h"

/**
* This function formats a time_t value into a human-readable string.
* @param t The time_t value to format.
* @return A pointer to a static buffer containing the formatted time string.
*/
char *format_time(time_t t) {
    static char buf[64];
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", localtime(&t));
    return buf;
}
