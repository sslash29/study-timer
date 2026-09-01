#ifndef QUIR_DURATION_H
#define QUIR_DURATION_H

/*
 * Parses free-form duration strings into a whole number of seconds.
 * Accepts: "5s", "10m", "1h", "1h30m", "1h 30m 15s", "90" (bare number = seconds).
 * Units recognized (case-insensitive): h/hr/hrs/hour/hours, m/min/mins/minute/minutes,
 * s/sec/secs/second/seconds.
 * Returns 0 on success and writes the total to *out_seconds.
 * Returns -1 on a malformed string (out_seconds is left untouched).
 */
int duration_parse(const char *str, long *out_seconds);

#endif
