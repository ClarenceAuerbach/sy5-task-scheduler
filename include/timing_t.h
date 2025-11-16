#ifndef TIMING_T
#define TIMING_T

#include <stdint.h>
#include <time.h>

/* bitmap representing what times a command should be run at
 * if the 8th and 16th hours are marked,
 * and the 0th (Sunday) and 5th (Friday) days are marked
 * then it'll run Monday 8:00, Monday 16:00, Friday 8:00, Friday 16:00
 */
typedef struct {
    uint64_t minutes;
    uint32_t hours;
    uint8_t daysofweek;
} timing_t;

// Finds the lowest place value bit set to 1, from start (inclusive) to end (exclusive)
int next_set_bit(uint64_t bitmap, int start, int end);

// Finds the next time_t described by timing_t
// Returns -1 on error:
// Either timing_t describes no valid time (one of the fields is null)
//  or the next timing overflows time_t
time_t next_exec_time(timing_t timing, time_t start_time);

// Checks whether a given time_t is 'now'
// Returns 1 if it has taken place within precision seconds
// (t <= now < t + precision)
// Otherwise 0
int check_time(time_t t, int precision);

#endif
