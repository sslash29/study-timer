#include "timer.h"
#include <stdio.h>
#include <string.h>

void timer_init(qtimer_t *t) {
    memset(t, 0, sizeof(*t));
    t->state = TIMER_IDLE;
}

void timer_start(qtimer_t *t, const char *task) {
    memset(t, 0, sizeof(*t));
    snprintf(t->task, sizeof(t->task), "%s", task);
    t->start_epoch = time(NULL);
    t->elapsed_base = 0;
    t->segment_start = t->start_epoch;
    t->state = TIMER_RUNNING;
}

void timer_pause(qtimer_t *t) {
    if (t->state != TIMER_RUNNING) return;
    time_t now = time(NULL);
    t->elapsed_base += (long)(now - t->segment_start);
    t->state = TIMER_PAUSED;
}

void timer_resume(qtimer_t *t) {
    if (t->state != TIMER_PAUSED) return;
    t->segment_start = time(NULL);
    t->state = TIMER_RUNNING;
}

long timer_elapsed(const qtimer_t *t) {
    if (t->state == TIMER_RUNNING) {
        time_t now = time(NULL);
        return t->elapsed_base + (long)(now - t->segment_start);
    }
    return t->elapsed_base;
}

void timer_adjust(qtimer_t *t, long delta_seconds) {
    if (t->state == TIMER_IDLE) return;
    if (t->state == TIMER_RUNNING) {
        /* fold the live segment into the base, apply delta, start a fresh segment */
        time_t now = time(NULL);
        t->elapsed_base += (long)(now - t->segment_start);
        t->segment_start = now;
    }
    t->elapsed_base += delta_seconds;
    if (t->elapsed_base < 0) t->elapsed_base = 0;
}

void timer_stop(qtimer_t *t, char *task_out, size_t task_out_sz, long *elapsed_out,
                time_t *start_out, time_t *end_out) {
    long final_elapsed = timer_elapsed(t);
    if (task_out) snprintf(task_out, task_out_sz, "%s", t->task);
    if (elapsed_out) *elapsed_out = final_elapsed;
    if (start_out) *start_out = t->start_epoch;
    if (end_out) *end_out = time(NULL);
    timer_init(t);
}

void timer_restore(qtimer_t *t, const state_snapshot_t *snap) {
    memset(t, 0, sizeof(*t));
    snprintf(t->task, sizeof(t->task), "%s", snap->task);
    t->start_epoch = snap->start_epoch;
    t->elapsed_base = snap->elapsed_base;
    t->segment_start = snap->segment_start;
    t->state = snap->is_paused ? TIMER_PAUSED : TIMER_RUNNING;
}

void timer_to_snapshot(const qtimer_t *t, state_snapshot_t *snap) {
    memset(snap, 0, sizeof(*snap));
    if (t->state == TIMER_IDLE) {
        snap->valid = 0;
        return;
    }
    snap->valid = 1;
    snprintf(snap->task, sizeof(snap->task), "%s", t->task);
    snap->start_epoch = t->start_epoch;
    snap->elapsed_base = t->elapsed_base;
    snap->segment_start = t->segment_start;
    snap->is_paused = (t->state == TIMER_PAUSED);
}
