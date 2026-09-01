#ifndef QUIR_STORAGE_H
#define QUIR_STORAGE_H

#include <time.h>

#define TASK_TEXT_MAX 256

typedef struct {
    long line_no;      /* index into sessions.log, for deletion */
    char date[11];
    char start_hms[9];
    char end_hms[9];
    long elapsed;       /* active seconds */
    char task[TASK_TEXT_MAX];
} session_t;

typedef struct {
    long line_no;      /* index into tasks.log, for toggle/delete/edit */
    char date[11];
    int done;
    char text[TASK_TEXT_MAX];
} task_t;

typedef struct {
    int valid;
    char task[TASK_TEXT_MAX];
    time_t start_epoch;
    long elapsed_base;
    time_t segment_start;
    int is_paused;
} state_snapshot_t;

/* Ensures ~/.quir exists. Returns 0 on success. */
int storage_init(void);

/* --- Sessions --- */
int storage_append_session(const char *date, const char *start_hms, const char *end_hms,
                            long elapsed, const char *task);
/* Caller must free() the returned array. *count set to number of items. */
session_t *storage_get_sessions_for_date(const char *date, int *count);
int storage_delete_session_line(long line_no);
/* totals[1..days_in_month] filled with seconds tracked that day. totals must hold 32 longs. */
void storage_get_month_totals(int year, int month, long totals[32]);
/* Sums elapsed seconds across sessions matching `date`. */
long storage_get_day_total(const char *date);
/* For each of the n dates, writes the total seconds into totals[i]. Single file pass. */
void storage_get_totals_for_dates(const char dates[][11], int n, long *totals);

/* --- Tasks --- */
task_t *storage_get_tasks_for_date(const char *date, int *count);
int storage_add_task(const char *date, const char *text);
int storage_toggle_task(long line_no);
int storage_delete_task_line(long line_no);
int storage_edit_task_line(long line_no, const char *new_text);

/* --- In-progress session snapshot (crash / quit resume) --- */
int storage_state_save(const state_snapshot_t *snap);
int storage_state_load(state_snapshot_t *snap); /* returns 1 if a valid snapshot was found */
int storage_state_clear(void);

#endif
