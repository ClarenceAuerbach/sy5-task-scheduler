#include <assert.h>
#include <stdio.h>

#include "timing_t.h"

int main() {
    // ~~~~~ Test check_time ~~~~~
    {
        time_t now;
        time(&now);
        time_t then = now - 5;
        assert(check_time(then, 1) == 0);
        then += 5; // Should take less than a second to execute
        assert(check_time(then, 1) == 1);
        then += 6;
        assert(check_time(then, 5) == 0);
    }
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
        assert(next_exec_time(timing, time(NULL)) == -1);
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
        time_t next_time = next_exec_time(timing, time(NULL));
        struct tm *tm = localtime(&next_time);
        
        // Time should always fall on the first second (0)
        assert(tm->tm_sec == 0);
        assert(tm->tm_min == 25);
        assert(tm->tm_hour == 23);
        assert(tm->tm_wday == 0);
    }

    // ~~~~~ Somewhat complicated test ~~~~~
    {
        timing_t timing = {
            .minutes = (1 << 15) | (1 << 30),
            .hours = (1 << 10) | (1 << 23),
            .daysofweek = 0x7F, // Every day of the week (simpler)
        };

        // Set a time later than any scheduled time
        time_t now;
        time(&now);
        struct tm tm = *localtime(&now);
        tm.tm_hour = 23;
        tm.tm_min = 45;
        tm.tm_sec = 0;
        time_t fake_time = mktime(&tm);

        // Find the next execution date starting from fake_time
        time_t next = next_exec_time(timing, fake_time);
        struct tm nt = *localtime(&next);

        assert(nt.tm_sec == 0);
        assert(nt.tm_hour == 10);
        assert(nt.tm_min == 15);
        // next_time's day is one more, or start of a year (edge case)
        assert((nt.tm_yday == tm.tm_yday+1) || nt.tm_yday == 0);
    }
}
