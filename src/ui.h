#ifndef QUIR_UI_H
#define QUIR_UI_H

#include <curses.h>
#include "timer.h"
#include "storage.h"

typedef enum { FOCUS_TIMER, FOCUS_TASKS } focus_t;

void ui_init_colors(void);

/* Home screen: timer panel + today's task list. anim_tick drives the pulse.
 * today_sessions/today_session_count are today's logged sessions, used to show
 * each task's tracked time beside it (matched to a task by exact task text). */
void ui_draw_home(const qtimer_t *timer, focus_t focus,
                   const task_t *tasks, int task_count, int task_sel,
                   const session_t *today_sessions, int today_session_count,
                   long today_total, const long week_totals[7],
                   const char *toast, int anim_tick);

/* Month calendar. totals must have 32 longs (index = day of month). */
void ui_draw_calendar(int year, int month, const long totals[32], int sel_day,
                       const char *toast);

/* Sessions for a single date. */
void ui_draw_day_detail(const char *date, const session_t *sessions, int count,
                         int sel, int tasks_done, int tasks_total, const char *toast);

/* --- Modals (block until answered) --- */
/* Returns 1 if confirmed (buf holds trimmed text), 0 if cancelled (Esc). */
int ui_modal_text(const char *title, const char *initial, char *buf, size_t bufsize);
/* Returns 1 for yes, 0 for no/cancel. */
int ui_modal_confirm(const char *message);
/* Presents `n` single-key options (labels[i] / keys[i]); returns chosen index, or -1 on Esc. */
int ui_modal_choice(const char *title, const char *message, const char *labels[],
                     const char keys[], int n);

#endif
