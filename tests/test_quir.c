#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "duration.h"
#include "dateutil.h"

static int failures = 0;

#define CHECK(cond, desc) do { \
    if (!(cond)) { printf("FAIL: %s\n", desc); failures++; } \
    else printf("ok:   %s\n", desc); \
} while (0)

static void test_duration(void) {
    long s;

    CHECK(duration_parse("5s", &s) == 0 && s == 5, "5s -> 5");
    CHECK(duration_parse("10m", &s) == 0 && s == 600, "10m -> 600");
    CHECK(duration_parse("1h", &s) == 0 && s == 3600, "1h -> 3600");
    CHECK(duration_parse("1h30m", &s) == 0 && s == 5400, "1h30m -> 5400");
    CHECK(duration_parse("1h 30m 15s", &s) == 0 && s == 5415, "1h 30m 15s -> 5415");
    CHECK(duration_parse("90", &s) == 0 && s == 90, "bare 90 -> 90 seconds");
    CHECK(duration_parse("2hr", &s) == 0 && s == 7200, "2hr -> 7200");
    CHECK(duration_parse("2hours", &s) == 0 && s == 7200, "2hours -> 7200");
    CHECK(duration_parse("45mins", &s) == 0 && s == 2700, "45mins -> 2700");
    CHECK(duration_parse("", &s) == -1, "empty string rejected");
    CHECK(duration_parse("abc", &s) == -1, "garbage rejected");
    CHECK(duration_parse("5x", &s) == -1, "bad unit rejected");
    CHECK(duration_parse("1h 30", &s) == -1, "unit-less trailing token rejected");
}

static void test_dateutil(void) {
    char out[11];

    CHECK(dt_days_in_month(2024, 2) == 29, "2024 is a leap year (Feb=29)");
    CHECK(dt_days_in_month(2023, 2) == 28, "2023 is not a leap year (Feb=28)");
    CHECK(dt_days_in_month(2000, 2) == 29, "2000 is a leap year (div by 400)");
    CHECK(dt_days_in_month(1900, 2) == 28, "1900 is not a leap year (div by 100 not 400)");

    dt_add_days("2026-08-29", 1, out);
    CHECK(strcmp(out, "2026-08-30") == 0, "2026-08-29 + 1 day");

    dt_add_days("2026-08-31", 1, out);
    CHECK(strcmp(out, "2026-09-01") == 0, "month rollover");

    dt_add_days("2026-12-31", 1, out);
    CHECK(strcmp(out, "2027-01-01") == 0, "year rollover");

    dt_add_days("2026-09-01", -1, out);
    CHECK(strcmp(out, "2026-08-31") == 0, "backwards month rollover");

    int y, m;
    dt_add_months(2026, 12, 1, &y, &m);
    CHECK(y == 2027 && m == 1, "month +1 rolls year");

    dt_add_months(2026, 1, -1, &y, &m);
    CHECK(y == 2025 && m == 12, "month -1 rolls year backwards");

    char hms[9];
    dt_format_hms(3661, hms);
    CHECK(strcmp(hms, "01:01:01") == 0, "3661s -> 01:01:01");

    char human[32];
    dt_format_human(3900, human, sizeof(human));
    CHECK(strcmp(human, "1h 5m") == 0, "3900s human -> 1h 5m");
    dt_format_human(125, human, sizeof(human));
    CHECK(strcmp(human, "2m 5s") == 0, "125s human -> 2m 5s");
    dt_format_human(9, human, sizeof(human));
    CHECK(strcmp(human, "9s") == 0, "9s human -> 9s");

    /* Aug 1 2026 is a Saturday -> Mon=0..Sun=6, Saturday = 5 */
    CHECK(dt_weekday_of(2026, 8, 1) == 5, "Aug 1 2026 is a Saturday (index 5)");
}

int main(void) {
    test_duration();
    test_dateutil();
    printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
