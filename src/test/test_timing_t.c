#include <assert.h>
#include <stdio.h>

#include "timing_t.h"

int main() {
    // ~~~~~ Test where nothing should work ~~~~~
    {
        timing_t timing = {
            .minutes = 0,
            .hours = 0,
            .daysofweek = 0,
        };
        for (int i = 0; i < 60; i++) {
            assert(next_set_bit(timing.minutes, i, 60) == -1);
            assert(next_set_bit(timing.hours, i, 24) == -1);
            assert(next_set_bit(timing.hours, i, 7) == -1);
        }
        assert(next_exec_time(timing) == -1);
    }

    // ~~~~~ Test where only one time exists ~~~~~
    {
        timing_t timing = {
            .minutes = 1 << 25, // Somewhere in the middle
            .hours = 1 << 23, // Last bit
            .daysofweek = 1, // First bit
        };
        assert(next_set_bit(timing.minutes, 0, 60) == 25);
        assert(next_set_bit(timing.hours, 0, 24) == 23);
        assert(next_set_bit(timing.daysofweek, 0, 7) == 0);
        // Checking to make sure the time has correct
        //  days, hours, minutes, seconds
        time_t next_time = next_exec_time(timing);
        struct tm *tm = localtime(&next_time);
        
        // Time should always fall on the first second (0)
        assert(tm->tm_sec == 0);
        assert(tm->tm_min == 25);
        assert(tm->tm_hour == 23);
        assert(tm->tm_wday == 0);
    }
}
