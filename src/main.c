#include <curses.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dateutil.h"
#include "duration.h"
#include "storage.h"
#include "timer.h"
#include "ui.h"

#define TICK_MS 200
#define TOAST_TICKS (4000 / TICK_MS)

typedef enum { SCREEN_HOME, SCREEN_CALENDAR, SCREEN_DAY_DETAIL } screen_t;

/* ---- app state ---- */
static qtimer_t g_timer;
static screen_t g_screen = SCREEN_HOME;
static focus_t g_focus = FOCUS_TASKS;

static char g_today[11];
static task_t *g_tasks = NULL;
static int g_task_count = 0;
static int g_task_sel = 0;

static long g_today_total = 0;
static long g_week_totals[7];
static session_t *g_today_sessions = NULL;
static int g_today_session_count = 0;

static int g_cal_year, g_cal_month, g_cal_sel_day;
static long g_cal_totals[32];

static char g_dd_date[11];
static session_t *g_dd_sessions = NULL;
static int g_dd_count = 0;
static int g_dd_sel = 0;
static int g_dd_tasks_done = 0, g_dd_tasks_total = 0;

static char g_toast[128] = "";
static int g_toast_ttl = 0;

static void toast_set(const char *msg) {
    snprintf(g_toast, sizeof(g_toast), "%s", msg);
    g_toast_ttl = TOAST_TICKS;
}

static void persist_snapshot(void) {
    state_snapshot_t snap;
    timer_to_snapshot(&g_timer, &snap);
    if (snap.valid) storage_state_save(&snap);
    else storage_state_clear();
}

static void reload_tasks(void) {
    free(g_tasks);
    g_tasks = storage_get_tasks_for_date(g_today, &g_task_count);
    if (g_task_sel >= g_task_count) g_task_sel = g_task_count > 0 ? g_task_count - 1 : 0;
    if (g_task_sel < 0) g_task_sel = 0;
}

static void reload_stats(void) {
    g_today_total = storage_get_day_total(g_today);
    char dates[7][11];
    for (int i = 0; i < 7; i++) dt_add_days(g_today, -(6 - i), dates[i]);
    storage_get_totals_for_dates(dates, 7, g_week_totals);
}

static void reload_today_sessions(void) {
    free(g_today_sessions);
    g_today_sessions = storage_get_sessions_for_date(g_today, &g_today_session_count);
}

static void reload_cal_totals(void) {
    storage_get_month_totals(g_cal_year, g_cal_month, g_cal_totals);
}

static void free_day_detail(void) {
    free(g_dd_sessions);
    g_dd_sessions = NULL;
    g_dd_count = 0;
}

static void load_day_detail(const char *date) {
    /* Copy first: callers may pass g_dd_date itself (e.g. reloading after a
     * delete), and snprintf's behavior is undefined when src and dest alias. */
    char date_copy[11];
    snprintf(date_copy, sizeof(date_copy), "%s", date);

    free_day_detail();
    snprintf(g_dd_date, sizeof(g_dd_date), "%s", date_copy);
    g_dd_sessions = storage_get_sessions_for_date(date_copy, &g_dd_count);
    g_dd_sel = 0;

    int tcount;
    task_t *tk = storage_get_tasks_for_date(date_copy, &tcount);
    int done = 0;
    for (int i = 0; i < tcount; i++) if (tk[i].done) done++;
    free(tk);
    g_dd_tasks_done = done;
    g_dd_tasks_total = tcount;
}

static void cal_goto(const char *date) {
    int y, m, d;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    if (y != g_cal_year || m != g_cal_month) {
        g_cal_year = y;
        g_cal_month = m;
        reload_cal_totals();
    }
    g_cal_sel_day = d;
}

static void cal_shift_month(int delta) {
    int y, m;
    dt_add_months(g_cal_year, g_cal_month, delta, &y, &m);
    g_cal_year = y;
    g_cal_month = m;
    int dim = dt_days_in_month(y, m);
    if (g_cal_sel_day > dim) g_cal_sel_day = dim;
    reload_cal_totals();
}

static void end_current_session(void) {
    char task[TASK_TEXT_MAX];
    long elapsed;
    time_t start, end;
    timer_stop(&g_timer, task, sizeof(task), &elapsed, &start, &end);

    char date[11], start_hms[9], end_hms[9];
    dt_date_from_epoch(start, date);
    dt_hms_from_epoch(start, start_hms);
    dt_hms_from_epoch(end, end_hms);
    storage_append_session(date, start_hms, end_hms, elapsed, task);
    storage_state_clear();

    reload_stats();
    reload_today_sessions();
    reload_tasks();
    reload_cal_totals();
    toast_set("Session saved.");
}

static void handle_startup_resume(void) {
    state_snapshot_t snap;
    if (!storage_state_load(&snap)) return;

    long live_elapsed = snap.elapsed_base + (snap.is_paused ? 0 : (long)(time(NULL) - snap.segment_start));
    char elapsed_buf[32];
    dt_format_human(live_elapsed, elapsed_buf, sizeof(elapsed_buf));
    char msg[300];
    snprintf(msg, sizeof(msg), "Resume \"%.60s\" (%s elapsed)?", snap.task, elapsed_buf);

    long zero_week[7] = {0};
    ui_draw_home(&g_timer, g_focus, NULL, 0, 0, NULL, 0, 0, zero_week, NULL, 0);

    const char *labels[3] = {"Resume it", "Save & end it now", "Discard it"};
    const char keys[3] = {'r', 's', 'd'};
    int choice = ui_modal_choice("Welcome back", msg, labels, keys, 3);

    if (choice == 0) {
        timer_restore(&g_timer, &snap);
        g_focus = FOCUS_TIMER;
    } else if (choice == 1) {
        char date[11], start_hms[9], end_hms[9];
        dt_date_from_epoch(snap.start_epoch, date);
        dt_hms_from_epoch(snap.start_epoch, start_hms);
        dt_hms_from_epoch(time(NULL), end_hms);
        storage_append_session(date, start_hms, end_hms, live_elapsed, snap.task);
        storage_state_clear();
    } else if (choice == 2) {
        storage_state_clear();
    }
    /* Esc (-1): leave the snapshot on disk untouched and start idle; we'll ask again next launch. */
}

static void do_quit(void) {
    persist_snapshot();
}

int main(void) {
    setlocale(LC_ALL, "");
    storage_init();
    timer_init(&g_timer);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    ui_init_colors();

    handle_startup_resume();

    dt_today_str(g_today);
    reload_tasks();
    reload_stats();
    reload_today_sessions();

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    g_cal_year = tmv.tm_year + 1900;
    g_cal_month = tmv.tm_mon + 1;
    g_cal_sel_day = tmv.tm_mday;
    reload_cal_totals();

    int anim_tick = 0;
    int running = 1;

    while (running) {
        char nowdate[11];
        dt_today_str(nowdate);
        if (strcmp(nowdate, g_today) != 0) {
            snprintf(g_today, sizeof(g_today), "%s", nowdate);
            reload_tasks();
            reload_stats();
            reload_today_sessions();
        }

        if (g_toast_ttl > 0) {
            g_toast_ttl--;
            if (g_toast_ttl == 0) g_toast[0] = '\0';
        }

        switch (g_screen) {
            case SCREEN_HOME:
                ui_draw_home(&g_timer, g_focus, g_tasks, g_task_count, g_task_sel,
                             g_today_sessions, g_today_session_count,
                             g_today_total, g_week_totals, g_toast[0] ? g_toast : NULL, anim_tick);
                break;
            case SCREEN_CALENDAR:
                ui_draw_calendar(g_cal_year, g_cal_month, g_cal_totals, g_cal_sel_day,
                                  g_toast[0] ? g_toast : NULL);
                break;
            case SCREEN_DAY_DETAIL:
                ui_draw_day_detail(g_dd_date, g_dd_sessions, g_dd_count, g_dd_sel,
                                    g_dd_tasks_done, g_dd_tasks_total, g_toast[0] ? g_toast : NULL);
                break;
        }

        timeout(TICK_MS);
        int ch = getch();
        if (ch == ERR) { anim_tick++; continue; }
        if (ch == KEY_RESIZE) continue;
        anim_tick++;

        if (g_screen == SCREEN_HOME) {
            if (ch == 'q' || ch == 'Q') {
                if (g_timer.state != TIMER_IDLE) {
                    if (ui_modal_confirm("Quit? Your session stays saved and resumable.")) {
                        do_quit();
                        running = 0;
                    }
                } else {
                    do_quit();
                    running = 0;
                }
            } else if (ch == 'c') {
                reload_cal_totals(); /* sessions may have changed since the calendar was last shown */
                g_screen = SCREEN_CALENDAR;
            } else if (ch == '\t') {
                g_focus = (g_focus == FOCUS_TIMER) ? FOCUS_TASKS : FOCUS_TIMER;
            } else if (g_focus == FOCUS_TIMER) {
                if (ch == ' ') {
                    if (g_timer.state == TIMER_RUNNING) timer_pause(&g_timer);
                    else if (g_timer.state == TIMER_PAUSED) timer_resume(&g_timer);
                    persist_snapshot();
                } else if (ch == '+') {
                    if (g_timer.state == TIMER_IDLE) {
                        toast_set("No active session to adjust.");
                    } else {
                        char buf[64] = "";
                        if (ui_modal_text("Add time", "", buf, sizeof(buf)) && buf[0]) {
                            long secs;
                            if (duration_parse(buf, &secs) == 0) {
                                timer_adjust(&g_timer, secs);
                                persist_snapshot();
                                toast_set("Time added.");
                            } else {
                                toast_set("Couldn't parse that (try 5s, 10m, 1h30m).");
                            }
                        }
                    }
                } else if (ch == '-') {
                    if (g_timer.state == TIMER_IDLE) {
                        toast_set("No active session to adjust.");
                    } else {
                        char buf[64] = "";
                        if (ui_modal_text("Remove time", "", buf, sizeof(buf)) && buf[0]) {
                            long secs;
                            if (duration_parse(buf, &secs) == 0) {
                                timer_adjust(&g_timer, -secs);
                                persist_snapshot();
                                toast_set("Time removed.");
                            } else {
                                toast_set("Couldn't parse that (try 5s, 10m, 1h30m).");
                            }
                        }
                    }
                } else if (ch == 'e') {
                    if (g_timer.state == TIMER_IDLE) {
                        toast_set("No active session to end.");
                    } else {
                        end_current_session();
                    }
                } else if (ch == 'n') {
                    if (g_timer.state != TIMER_IDLE) {
                        toast_set("A session is already running.");
                    } else {
                        char buf[TASK_TEXT_MAX] = "";
                        if (ui_modal_text("What are you working on?", "", buf, sizeof(buf)) && buf[0]) {
                            timer_start(&g_timer, buf);
                            persist_snapshot();
                        }
                    }
                }
            } else { /* FOCUS_TASKS */
                if (ch == KEY_UP || ch == 'k') {
                    if (g_task_sel > 0) g_task_sel--;
                } else if (ch == KEY_DOWN || ch == 'j') {
                    if (g_task_sel < g_task_count - 1) g_task_sel++;
                } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                    if (g_task_count == 0) {
                        toast_set("Add a task first with [a].");
                    } else if (g_timer.state != TIMER_IDLE) {
                        toast_set("A session is already running \xe2\x80\x94 end it first.");
                    } else {
                        timer_start(&g_timer, g_tasks[g_task_sel].text);
                        persist_snapshot();
                        g_focus = FOCUS_TIMER;
                    }
                } else if (ch == 'a') {
                    char buf[TASK_TEXT_MAX] = "";
                    if (ui_modal_text("New task", "", buf, sizeof(buf)) && buf[0]) {
                        storage_add_task(g_today, buf);
                        reload_tasks();
                        g_task_sel = g_task_count - 1;
                        toast_set("Task added.");
                    }
                } else if (ch == ' ') {
                    if (g_task_count > 0) {
                        storage_toggle_task(g_tasks[g_task_sel].line_no);
                        reload_tasks();
                    }
                } else if (ch == 'r') {
                    if (g_task_count > 0) {
                        char buf[TASK_TEXT_MAX];
                        snprintf(buf, sizeof(buf), "%s", g_tasks[g_task_sel].text);
                        if (ui_modal_text("Rename task", buf, buf, sizeof(buf)) && buf[0]) {
                            storage_edit_task_line(g_tasks[g_task_sel].line_no, buf);
                            reload_tasks();
                        }
                    }
                } else if (ch == 'd') {
                    if (g_task_count > 0) {
                        char msg[300];
                        snprintf(msg, sizeof(msg), "Delete task \"%.40s\"?", g_tasks[g_task_sel].text);
                        if (ui_modal_confirm(msg)) {
                            storage_delete_task_line(g_tasks[g_task_sel].line_no);
                            reload_tasks();
                            toast_set("Task deleted.");
                        }
                    }
                }
            }
        } else if (g_screen == SCREEN_CALENDAR) {
            char cur[11];
            snprintf(cur, sizeof(cur), "%04d-%02d-%02d", g_cal_year, g_cal_month, g_cal_sel_day);

            if (ch == 27 || ch == 'c' || ch == 'q') {
                g_screen = SCREEN_HOME;
            } else if (ch == KEY_LEFT || ch == 'h') {
                char nd[11]; dt_add_days(cur, -1, nd); cal_goto(nd);
            } else if (ch == KEY_RIGHT || ch == 'l') {
                char nd[11]; dt_add_days(cur, 1, nd); cal_goto(nd);
            } else if (ch == KEY_UP || ch == 'k') {
                char nd[11]; dt_add_days(cur, -7, nd); cal_goto(nd);
            } else if (ch == KEY_DOWN || ch == 'j') {
                char nd[11]; dt_add_days(cur, 7, nd); cal_goto(nd);
            } else if (ch == KEY_PPAGE || ch == '[') {
                cal_shift_month(-1);
            } else if (ch == KEY_NPAGE || ch == ']') {
                cal_shift_month(1);
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                load_day_detail(cur);
                g_screen = SCREEN_DAY_DETAIL;
            }
        } else if (g_screen == SCREEN_DAY_DETAIL) {
            if (ch == 27 || ch == 'q') {
                g_screen = SCREEN_CALENDAR;
            } else if (ch == KEY_UP || ch == 'k') {
                if (g_dd_sel > 0) g_dd_sel--;
            } else if (ch == KEY_DOWN || ch == 'j') {
                if (g_dd_sel < g_dd_count - 1) g_dd_sel++;
            } else if (ch == 'd') {
                if (g_dd_count > 0 && ui_modal_confirm("Delete this session entry?")) {
                    storage_delete_session_line(g_dd_sessions[g_dd_sel].line_no);
                    load_day_detail(g_dd_date);
                    if (strcmp(g_dd_date, g_today) == 0) {
                        reload_stats();
                        reload_today_sessions();
                    }
                    reload_cal_totals();
                    toast_set("Session entry deleted.");
                }
            }
        }
    }

    free(g_tasks);
    free(g_today_sessions);
    free_day_detail();
    endwin();
    return 0;
}
