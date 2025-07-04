#include <time.h>

#define TRUE 1
#define FALSE 0

/**
* This function formats a time_t value into a human-readable string.
* @param t The time_t value to format.
* @return A pointer to a static buffer containing the formatted time string.
*/
char *format_time(time_t t);
