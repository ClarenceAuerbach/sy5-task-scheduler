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
 Important note:
 time_t mktime(struct tm) will take the struct tm, renormalise it, and return the time_t equivalent
 Returns -1 on error:
 Either timing_t describes no valid time (one of the fields is null)
 or the next timing overflows time_t
*/
time_t next_exec_time(timing_t timing, time_t start_time) {
    // Must find a time strictly in the future
    // (Otherwise infinite execution of a task within the same second)
    start_time++;
    struct tm *src = localtime(&start_time);
    if (!src) return -1; // localtime error
    // Copy. src is a pointer to volatile static memory
    struct tm tm = *src;
    // Starting on a round minute
    if (tm.tm_sec != 0) {
    	tm.tm_min++;
	tm.tm_sec = 0;
	mktime(&tm);
    }
    while(1) {
        int next_day = next_set_bit(timing.daysofweek, tm.tm_wday, 7);
        if (next_day == -1) { // Wrap to next week
            next_day = next_set_bit(timing.daysofweek, 0, 7); // start from the start of the week
            if (next_day == -1) return -1; // No days at all
            // Prepares tm struct for the next loop
            tm.tm_mday += (next_day - tm.tm_wday) + 7;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            mktime(&tm);
            continue;
        }
	// update the day of the month (because wday is ignored in mktime)
        tm.tm_mday += next_day - tm.tm_wday;
	mktime(&tm);

        int next_hour = next_set_bit(timing.hours, tm.tm_hour, 24);
        if (next_hour == -1) {
            // Prepares tm struct for the next loop
            tm.tm_mday++;
            tm.tm_hour = 0;
            tm.tm_min = 0;
            mktime(&tm);
            continue;
        }
        tm.tm_hour = next_hour;

        int next_min = next_set_bit(timing.minutes, tm.tm_min, 60);
        if (next_min == -1) {
            tm.tm_hour++;
            tm.tm_min = 0;
            mktime(&tm);
            continue;
        }
        tm.tm_min = next_min;

        time_t res = mktime(&tm);
        return res;
    }
}

/* Checks whether a given time_t is 'now'
 Returns 1 if it has taken place within precision seconds
 (t <= now < t + precision)
 Otherwise 0
*/
int check_time(time_t t, int precision) {
    time_t now;
    time(&now);
    double diff = difftime(now, t); // Basically (now - t)

    if (diff < 0) // Too soon
        return 0;
    if (diff < precision) // t <= now < t+precision
        return 1;
    return 0; // Too late 
}

time_t min( time_t * times , int length){
    if(length == 0) return 0;
    time_t min = times[0];
    for(int i =0; i<length ; i++){
        if( times[i]<min){
            min = times[i];
        }
    }
    return min;
}


