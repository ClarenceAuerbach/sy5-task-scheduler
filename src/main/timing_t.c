#include <time.h>

#include "timing_t.h"

/* Helper function for next_exec_time
 Finds the next bit set to 1, starting from start
 */
int next_set_bit(uint64_t bitmap, int start, int end) {
    for (int i = start; i < end; i++) {
        if ((bitmap >> i) & 1) {
            return i;
        }
    }
    return -1; // No set bit found
}

/* Finds the next time_t described by timing_t
 (Strictly in the future)
 Returns -1 on error:
 Either timing_t describes no valid time (one of the fields is null)
 or the next timing overflows time_t
*/
time_t next_exec_time(timing_t timing, time_t start_time) {
    start_time++;
    struct tm *src = localtime(&start_time);
    if (!src) return -1; // localtime error
    struct tm tm = *src;
    // Starting on a round minute
    if (tm.tm_sec != 0) {
    	tm.tm_min++;
	tm.tm_sec = 0;
	mktime(&tm);
    }
    while (1) {
        int next_day = next_set_bit(timing.daysofweek, tm.tm_wday, 7);
        if (next_day == -1) {
            next_day = next_set_bit(timing.daysofweek, 0, 7);
            if (next_day == -1) return -1; // No days
            tm.tm_mday += next_day - tm.tm_wday + 7; // > 0
            tm.tm_hour = 0;
            tm.tm_min = 0;
        } else if (next_day != tm.tm_wday) { // Next day isn't today, but later in the week
            tm.tm_mday += next_day - tm.tm_wday;
            tm.tm_hour = 0;
            tm.tm_min = 0;
        }
        mktime(&tm);

        int next_hour = next_set_bit(timing.hours, tm.tm_hour, 24);
        if (next_hour == -1) {
            next_hour = next_set_bit(timing.hours, 0, 24);
            if (next_hour == -1) return -1; // No hours
            tm.tm_mday++; 
            tm.tm_hour = 0;
            tm.tm_min = 0;
            mktime(&tm);
            continue; // Recalculate from tomorrow onwards
        } else if (next_hour != tm.tm_hour) { // Next hour is later in the current day
            tm.tm_hour = next_hour;
            tm.tm_min = 0;
        }

        int next_min = next_set_bit(timing.minutes, tm.tm_min, 60);
        if (next_min == -1) {
            next_min = next_set_bit(timing.minutes, 0, 60);
            if (next_min == -1) return -1; // No minutes
            tm.tm_hour++;
            tm.tm_min = 0;
            mktime(&tm);
            continue; // Recalculate from the next hour onwards
        } else if (next_min != tm.tm_min) { // Next minute is later in the current day
            tm.tm_min = next_min;
        }
        break;
    }

    time_t res = mktime(&tm);
    return res;
}
