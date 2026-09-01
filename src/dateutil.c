#include "dateutil.h"
#include <stdio.h>
#include <string.h>

static const char *MONTH_NAMES[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *WEEKDAY_SHORT[] = {
    "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"
};

void dt_today_str(char out[11]) {
    time_t now = time(NULL);
    dt_date_from_epoch(now, out);
}

void dt_date_from_epoch(time_t t, char out[11]) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    memcpy(out, tmp, 11);
}

void dt_hms_from_epoch(time_t t, char out[9]) {
    struct tm tmv;
    localtime_r(&t, &tmv);
    snprintf(out, 9, "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
}

int dt_weekday_of(int year, int month, int day) {
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = year - 1900;
    tmv.tm_mon = month - 1;
    tmv.tm_mday = day;
    tmv.tm_hour = 12; /* noon avoids DST edge issues */
    time_t t = mktime(&tmv);
    struct tm out;
    localtime_r(&t, &out);
    /* tm_wday: 0=Sun..6=Sat -> convert to Mon=0..Sun=6 */
    int w = out.tm_wday;
    return (w == 0) ? 6 : (w - 1);
}

int dt_days_in_month(int year, int month) {
    static const int base[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return base[month - 1];
}

void dt_add_days(const char *date, int delta, char out[11]) {
    int y, m, d;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon = m - 1;
    tmv.tm_mday = d + delta;
    tmv.tm_hour = 12;
    time_t t = mktime(&tmv);
    dt_date_from_epoch(t, out);
}

void dt_add_months(int year, int month, int delta, int *out_year, int *out_month) {
    int total = (year * 12 + (month - 1)) + delta;
    *out_year = total / 12;
    *out_month = (total % 12) + 1;
    if (*out_month <= 0) {
        *out_month += 12;
        (*out_year)--;
    }
}

int dt_is_today(const char *date) {
    char today[11];
    dt_today_str(today);
    return strcmp(date, today) == 0;
}

int dt_compare(const char *a, const char *b) {
    return strcmp(a, b);
}

void dt_format_hms(long total_seconds, char out[9]) {
    if (total_seconds < 0) total_seconds = 0;
    long h = total_seconds / 3600;
    long m = (total_seconds % 3600) / 60;
    long s = total_seconds % 60;
    if (h > 99) h = 99;
    snprintf(out, 9, "%02ld:%02ld:%02ld", h, m, s);
}

void dt_format_human(long total_seconds, char *out, size_t outsz) {
    if (total_seconds < 0) total_seconds = 0;
    long h = total_seconds / 3600;
    long m = (total_seconds % 3600) / 60;
    long s = total_seconds % 60;
    if (h > 0) {
        snprintf(out, outsz, "%ldh %ldm", h, m);
    } else if (m > 0) {
        snprintf(out, outsz, "%ldm %lds", m, s);
    } else {
        snprintf(out, outsz, "%lds", s);
    }
}

const char *dt_month_name(int month) {
    if (month < 1 || month > 12) return "";
    return MONTH_NAMES[month];
}

const char *dt_weekday_short(int weekday0mon) {
    if (weekday0mon < 0 || weekday0mon > 6) return "??";
    return WEEKDAY_SHORT[weekday0mon];
}
