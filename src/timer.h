#ifndef QUIR_TIMER_H
#define QUIR_TIMER_H

#include <time.h>
#include "storage.h"

typedef enum {
    TIMER_IDLE = 0,
    TIMER_RUNNING,
    TIMER_PAUSED
} timer_state_t;

typedef struct {
    timer_state_t state;
    char task[TASK_TEXT_MAX];
    time_t start_epoch;     /* when the session first started */
    long elapsed_base;      /* accumulated active seconds, excluding the live segment */
    time_t segment_start;   /* epoch when the current running segment began */
} qtimer_t;

void timer_init(qtimer_t *t);
void timer_start(qtimer_t *t, const char *task);
void timer_pause(qtimer_t *t);
void timer_resume(qtimer_t *t);
long timer_elapsed(const qtimer_t *t);
/* delta may be negative (remove time); result is clamped to >= 0. */
void timer_adjust(qtimer_t *t, long delta_seconds);
/* Ends the session, filling out_* with final values. Resets *t to idle. */
void timer_stop(qtimer_t *t, char *task_out, size_t task_out_sz, long *elapsed_out,
                 time_t *start_out, time_t *end_out);

/* Restores a timer from a saved snapshot (used for crash/quit recovery). */
void timer_restore(qtimer_t *t, const state_snapshot_t *snap);
void timer_to_snapshot(const qtimer_t *t, state_snapshot_t *snap);

#endif
