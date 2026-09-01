#ifndef QUIR_DATEUTIL_H
#define QUIR_DATEUTIL_H

#include <time.h>

/* All date strings are "YYYY-MM-DD", all time strings are "HH:MM:SS". */

void dt_today_str(char out[11]);
void dt_date_from_epoch(time_t t, char out[11]);
void dt_hms_from_epoch(time_t t, char out[9]);

/* Monday = 0 .. Sunday = 6 */
int dt_weekday_of(int year, int month, int day);
int dt_days_in_month(int year, int month);

/* Shifts a YYYY-MM-DD date by delta days (may cross month/year boundaries). */
void dt_add_days(const char *date, int delta, char out[11]);

/* Shifts (year, month) by delta months, normalizing year. */
void dt_add_months(int year, int month, int delta, int *out_year, int *out_month);

int dt_is_today(const char *date);
int dt_compare(const char *a, const char *b); /* like strcmp */

/* "HH:MM:SS" from a seconds count (not a timestamp). */
void dt_format_hms(long total_seconds, char out[9]);

/* Compact human duration, e.g. "2h 15m", "45m", "30s". */
void dt_format_human(long total_seconds, char *out, size_t outsz);

const char *dt_month_name(int month); /* 1-12 */
const char *dt_weekday_short(int weekday0mon); /* 0=Mon..6=Sun */

#endif
