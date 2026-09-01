#include "duration.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int unit_seconds(const char *unit, long *mult) {
    if (strcmp(unit, "h") == 0 || strcmp(unit, "hr") == 0 || strcmp(unit, "hrs") == 0 ||
        strcmp(unit, "hour") == 0 || strcmp(unit, "hours") == 0) {
        *mult = 3600;
        return 1;
    }
    if (strcmp(unit, "m") == 0 || strcmp(unit, "min") == 0 || strcmp(unit, "mins") == 0 ||
        strcmp(unit, "minute") == 0 || strcmp(unit, "minutes") == 0) {
        *mult = 60;
        return 1;
    }
    if (strcmp(unit, "s") == 0 || strcmp(unit, "sec") == 0 || strcmp(unit, "secs") == 0 ||
        strcmp(unit, "second") == 0 || strcmp(unit, "seconds") == 0) {
        *mult = 1;
        return 1;
    }
    return 0;
}

int duration_parse(const char *str, long *out_seconds) {
    if (!str) return -1;
    const char *p = str;
    long total = 0;
    int found_number = 0;
    int first_token = 1;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        if (!isdigit((unsigned char)*p)) return -1;

        char *end;
        long value = strtol(p, &end, 10);
        if (end == p) return -1;
        p = end;

        while (*p == ' ' || *p == '\t') p++;

        char unit[16];
        size_t ulen = 0;
        while (*p && isalpha((unsigned char)*p) && ulen < sizeof(unit) - 1) {
            unit[ulen++] = (char)tolower((unsigned char)*p);
            p++;
        }
        unit[ulen] = '\0';

        found_number = 1;

        if (ulen == 0) {
            /* Bare number: only valid as the sole token in the whole string. */
            while (*p == ' ' || *p == '\t') p++;
            if (!first_token || *p != '\0') return -1;
            total += value;
            break;
        }

        long mult;
        if (!unit_seconds(unit, &mult)) return -1;
        total += value * mult;
        first_token = 0;
    }

    if (!found_number) return -1;
    if (total < 0) return -1;

    *out_seconds = total;
    return 0;
}
