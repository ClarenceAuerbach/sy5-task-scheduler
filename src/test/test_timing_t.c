#include <assert.h>

#include "timing_t.h"

int main() {
    // ~~~~~ Test next_set_bit ~~~~~
    {
        for (int i = 0; i < 60; i++) {
            for (int start = 0; start <= i; start++) {
                assert(next_set_bit((uint64_t)1<<i, start, 60) == i);
            }
        }
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

    // ~~~~~ Tests where only one time exists ~~~~~
    {
        time_t now = time(NULL);

        for (int min = 0; min < 60; min++) {
        for (int hour = 0; hour < 24; hour++) {
        for (int day = 0; day < 7; day++) {
            timing_t timing = {
                .minutes = (uint64_t)1 << min,
                .hours = (uint32_t)1 << hour,
                .daysofweek = (uint8_t)1 << day,
            };
            assert(next_set_bit(timing.minutes, 0, 60) == min);
            assert(next_set_bit(timing.hours, 0, 24) == hour);
            assert(next_set_bit(timing.daysofweek, 0, 7) == day);
            time_t next_time = next_exec_time(timing, now);
            struct tm *tm = localtime(&next_time);
            assert(next_time > now);
            assert(next_time - now <= 604800); // Not more than one week in the future
            assert(tm->tm_sec == 0);
            assert(tm->tm_min == min);
            assert(tm->tm_hour == hour);
            assert(tm->tm_wday == day);
        }
        }
        }
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
