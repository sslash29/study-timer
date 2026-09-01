#include "ui.h"
#include "bigfont.h"
#include "dateutil.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MIN_COLS 76
#define MIN_LINES 24

enum {
    CP_TITLE = 1, CP_ACCENT, CP_DIM, CP_TEXT,
    CP_RUNNING, CP_PAUSED, CP_IDLE, CP_DANGER,
    CP_BORDER_FOCUS, CP_BORDER_DIM, CP_SELECT, CP_DONE,
    CP_FOOTER, CP_MODAL,
    CP_HEAT0, CP_HEAT1, CP_HEAT2, CP_HEAT3, CP_HEAT4,
    CP_SELECT_ACCENT, CP_SELECT_RUN, CP_SELECT_PAUSE,
};

static int has256 = 0;

void ui_init_colors(void) {
    start_color();
    use_default_colors();
    has256 = (COLORS >= 256);

    if (has256) {
        init_pair(CP_TITLE, 135, -1);
        init_pair(CP_ACCENT, 51, -1);
        init_pair(CP_DIM, 244, -1);
        init_pair(CP_TEXT, 253, -1);
        init_pair(CP_RUNNING, 78, -1);
        init_pair(CP_PAUSED, 214, -1);
        init_pair(CP_IDLE, 246, -1);
        init_pair(CP_DANGER, 203, -1);
        init_pair(CP_BORDER_FOCUS, 135, -1);
        init_pair(CP_BORDER_DIM, 239, -1);
        init_pair(CP_SELECT, 232, 255);
        init_pair(CP_DONE, 240, -1);
        init_pair(CP_FOOTER, 250, 236);
        init_pair(CP_MODAL, 253, 236);
        init_pair(CP_HEAT0, 240, 236);
        init_pair(CP_HEAT1, 255, 23);
        init_pair(CP_HEAT2, 255, 30);
        init_pair(CP_HEAT3, 232, 37);
        init_pair(CP_HEAT4, 232, 51);
        init_pair(CP_SELECT_ACCENT, 90, 255);
        init_pair(CP_SELECT_RUN, 28, 255);
        init_pair(CP_SELECT_PAUSE, 130, 255);
    } else {
        init_pair(CP_TITLE, COLOR_MAGENTA, -1);
        init_pair(CP_ACCENT, COLOR_CYAN, -1);
        init_pair(CP_DIM, COLOR_WHITE, -1);
        init_pair(CP_TEXT, COLOR_WHITE, -1);
        init_pair(CP_RUNNING, COLOR_GREEN, -1);
        init_pair(CP_PAUSED, COLOR_YELLOW, -1);
        init_pair(CP_IDLE, COLOR_WHITE, -1);
        init_pair(CP_DANGER, COLOR_RED, -1);
        init_pair(CP_BORDER_FOCUS, COLOR_MAGENTA, -1);
        init_pair(CP_BORDER_DIM, COLOR_WHITE, -1);
        init_pair(CP_SELECT, COLOR_BLACK, COLOR_WHITE);
        init_pair(CP_DONE, COLOR_WHITE, -1);
        init_pair(CP_FOOTER, COLOR_WHITE, COLOR_BLACK);
        init_pair(CP_MODAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(CP_HEAT0, COLOR_WHITE, -1);
        init_pair(CP_HEAT1, COLOR_GREEN, -1);
        init_pair(CP_HEAT2, COLOR_GREEN, -1);
        init_pair(CP_HEAT3, COLOR_CYAN, -1);
        init_pair(CP_HEAT4, COLOR_CYAN, -1);
        init_pair(CP_SELECT_ACCENT, COLOR_MAGENTA, COLOR_WHITE);
        init_pair(CP_SELECT_RUN, COLOR_GREEN, COLOR_WHITE);
        init_pair(CP_SELECT_PAUSE, COLOR_BLUE, COLOR_WHITE);
    }
}

/* ---------- small drawing helpers ---------- */

/* Display width in terminal columns: counts UTF-8 codepoints, not bytes.
 * (All glyphs this app draws — box lines, arrows, dots — are single-width.) */
static int disp_width(const char *s) {
    int width = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) width++;
    }
    return width;
}

static void put_centered(int y, int x0, int width, const char *s, int pair, attr_t attrs) {
    int len = disp_width(s);
    int x = x0 + (width - len) / 2;
    if (x < x0) x = x0;
    attron(COLOR_PAIR(pair) | attrs);
    mvprintw(y, x, "%s", s);
    attroff(COLOR_PAIR(pair) | attrs);
}

static void hline_full(int y, int x, int width, int pair) {
    attron(COLOR_PAIR(pair));
    for (int i = 0; i < width; i++) mvprintw(y, x + i, "\xe2\x94\x80"); /* ─ */
    attroff(COLOR_PAIR(pair));
}

static void draw_box(int y, int x, int h, int w, int pair, attr_t attrs) {
    attron(COLOR_PAIR(pair) | attrs);
    mvprintw(y, x, "\xe2\x95\xad"); /* ╭ */
    mvprintw(y, x + w - 1, "\xe2\x95\xae"); /* ╮ */
    mvprintw(y + h - 1, x, "\xe2\x95\xb0"); /* ╰ */
    mvprintw(y + h - 1, x + w - 1, "\xe2\x95\xaf"); /* ╯ */
    for (int i = 1; i < w - 1; i++) {
        mvprintw(y, x + i, "\xe2\x94\x80");         /* ─ top */
        mvprintw(y + h - 1, x + i, "\xe2\x94\x80"); /* ─ bottom */
    }
    for (int i = 1; i < h - 1; i++) {
        mvprintw(y + i, x, "\xe2\x94\x82");         /* │ left */
        mvprintw(y + i, x + w - 1, "\xe2\x94\x82"); /* │ right */
    }
    attroff(COLOR_PAIR(pair) | attrs);
}

static void truncate_str(const char *in, char *out, size_t outsz) {
    size_t len = strlen(in);
    if (len < outsz) {
        memcpy(out, in, len + 1);
        return;
    }
    if (outsz < 4) { out[0] = '\0'; return; }
    memcpy(out, in, outsz - 4);
    strcpy(out + outsz - 4, "...");
}

static void draw_toast(int y, int width, const char *toast) {
    if (!toast || !toast[0]) return;
    put_centered(y, 0, width, toast, CP_ACCENT, A_BOLD);
}

static int screen_too_small(void) {
    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        erase();
        const char *msg = "Please enlarge your terminal (needs at least 70x24)...";
        put_centered(LINES / 2, 0, COLS, msg, CP_DANGER, A_BOLD);
        refresh();
        return 1;
    }
    return 0;
}

/* ---------- home screen ---------- */

static long task_total_seconds(const char *task_text, const session_t *sessions, int count) {
    long total = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(sessions[i].task, task_text) == 0) total += sessions[i].elapsed;
    }
    return total;
}

void ui_draw_home(const qtimer_t *timer, focus_t focus,
                   const task_t *tasks, int task_count, int task_sel,
                   const session_t *today_sessions, int today_session_count,
                   long today_total, const long week_totals[7],
                   const char *toast, int anim_tick) {
    erase();
    if (screen_too_small()) return;

    int width = COLS;

    put_centered(1, 0, width, "Q U I R", CP_TITLE, A_BOLD);
    hline_full(2, 2, width - 4, CP_BORDER_DIM);

    /* ---- timer panel ---- */
    int panel_w = width - 4 < 74 ? width - 4 : 74;
    int panel_x = (width - panel_w) / 2;
    int timer_y = 4;
    int timer_h = 9;

    int timer_border_pair = (focus == FOCUS_TIMER) ? CP_BORDER_FOCUS : CP_BORDER_DIM;
    draw_box(timer_y, panel_x, timer_h, panel_w, timer_border_pair, focus == FOCUS_TIMER ? A_BOLD : A_NORMAL);

    const char *state_label;
    int state_pair;
    const char *dot = "\xe2\x97\x8f"; /* ● */
    switch (timer->state) {
        case TIMER_RUNNING: state_label = "RUNNING"; state_pair = CP_RUNNING; break;
        case TIMER_PAUSED:  state_label = "PAUSED";  state_pair = CP_PAUSED;  break;
        default:            state_label = "IDLE";    state_pair = CP_IDLE;   break;
    }

    char status_line[300];
    if (timer->state == TIMER_IDLE) {
        snprintf(status_line, sizeof(status_line), "%s  No session running", "\xe2\x97\x8b");
    } else {
        char taskbuf[80];
        truncate_str(timer->task, taskbuf, sizeof(taskbuf));
        snprintf(status_line, sizeof(status_line), "%s %s", state_label, taskbuf);
    }

    int dot_visible = 1;
    if (timer->state == TIMER_RUNNING) dot_visible = (anim_tick % 10) < 7;

    int status_y = timer_y + 1;
    if (timer->state != TIMER_IDLE) {
        int llen = disp_width(status_line) + 2;
        int sx = panel_x + (panel_w - llen) / 2;
        if (dot_visible) {
            attron(COLOR_PAIR(state_pair) | A_BOLD);
            mvprintw(status_y, sx, "%s", dot);
            attroff(COLOR_PAIR(state_pair) | A_BOLD);
        }
        attron(COLOR_PAIR(state_pair) | A_BOLD);
        mvprintw(status_y, sx + 2, "%s", status_line);
        attroff(COLOR_PAIR(state_pair) | A_BOLD);
    } else {
        put_centered(status_y, panel_x + 2, panel_w - 4, status_line, CP_DIM, A_NORMAL);
    }

    char clockstr[9];
    dt_format_hms(timer_elapsed(timer), clockstr);
    int bw = bigfont_width(clockstr);
    int bx = panel_x + (panel_w - bw) / 2;
    int by = timer_y + 2;
    int clock_pair = (timer->state == TIMER_RUNNING) ? CP_RUNNING
                     : (timer->state == TIMER_PAUSED) ? CP_PAUSED : CP_DIM;
    bigfont_draw(stdscr, by, bx, clockstr, clock_pair, A_BOLD);

    const char *hint;
    if (timer->state == TIMER_IDLE) hint = "Start a task below, or press [n] for a quick session";
    else if (timer->state == TIMER_RUNNING) hint = "[Space] pause   [+/-] adjust time   [e] end session";
    else hint = "[Space] resume   [+/-] adjust time   [e] end session";
    put_centered(timer_y + timer_h - 2, panel_x + 2, panel_w - 4, hint, CP_DIM, A_NORMAL);

    /* ---- tasks panel ---- */
    int tasks_y = timer_y + timer_h + 1;
    int tasks_h = LINES - tasks_y - 4;
    if (tasks_h < 5) tasks_h = 5;
    int tasks_border_pair = (focus == FOCUS_TASKS) ? CP_BORDER_FOCUS : CP_BORDER_DIM;
    draw_box(tasks_y, panel_x, tasks_h, panel_w, tasks_border_pair, focus == FOCUS_TASKS ? A_BOLD : A_NORMAL);

    int done = 0;
    for (int i = 0; i < task_count; i++) if (tasks[i].done) done++;
    char header[64];
    snprintf(header, sizeof(header), " TODAY'S TASKS (%d/%d done) ", done, task_count);
    attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
    mvprintw(tasks_y, panel_x + 2, "%s", header);
    attroff(COLOR_PAIR(CP_ACCENT) | A_BOLD);

    int list_top = tasks_y + 2;
    int visible_rows = tasks_h - 4;
    if (visible_rows < 1) visible_rows = 1;

    if (task_count == 0) {
        put_centered(list_top, panel_x + 2, panel_w - 4, "No tasks yet. Press [a] to add one.", CP_DIM, A_NORMAL);
    } else {
        int start = 0;
        if (task_count > visible_rows) {
            start = task_sel - visible_rows / 2;
            if (start < 0) start = 0;
            if (start > task_count - visible_rows) start = task_count - visible_rows;
        }
        int end = start + visible_rows;
        if (end > task_count) end = task_count;

        for (int i = start; i < end; i++) {
            int row = list_top + (i - start);
            int is_sel = (focus == FOCUS_TASKS && i == task_sel);
            int is_active = (timer->state != TIMER_IDLE && strcmp(timer->task, tasks[i].text) == 0);

            int lx = panel_x + 3;
            int right_reserve = 19; /* room for the active dot + "[Enter=Start]" hint */
            int base_lw = panel_w - 6 - right_reserve;
            if (base_lw < 8) base_lw = 8;

            char durbuf[16] = "";
            long task_total = task_total_seconds(tasks[i].text, today_sessions, today_session_count);
            if (task_total > 0) dt_format_human(task_total, durbuf, sizeof(durbuf));
            int dur_w = durbuf[0] ? (int)strlen(durbuf) + 2 : 0; /* gap + duration */
            if (base_lw - dur_w < 8) dur_w = 0; /* not enough room; drop it */
            int lw = base_lw - dur_w;

            char line[300];
            char textbuf[200];
            truncate_str(tasks[i].text, textbuf, lw - 1 < (int)sizeof(textbuf) ? (size_t)(lw - 1) : sizeof(textbuf));
            const char *box = tasks[i].done ? "\xe2\x97\x89" : "\xe2\x97\x8b"; /* ◉ / ○ */
            snprintf(line, sizeof(line), "%s %s", box, textbuf);

            if (is_sel) {
                attron(COLOR_PAIR(CP_SELECT));
                mvprintw(row, panel_x + 1, "%*s", panel_w - 2, "");
                attroff(COLOR_PAIR(CP_SELECT));
            }

            int pair = tasks[i].done ? CP_DONE : (is_sel ? CP_SELECT : CP_TEXT);
            attr_t attrs = tasks[i].done ? A_DIM : A_NORMAL;
            attron(COLOR_PAIR(pair) | attrs);
            mvprintw(row, lx, "%.*s", lw, line);
            attroff(COLOR_PAIR(pair) | attrs);

            if (durbuf[0]) {
                int dur_pair = is_sel ? CP_SELECT : CP_DIM;
                attron(COLOR_PAIR(dur_pair) | A_NORMAL);
                mvprintw(row, lx + lw + 1, "%s", durbuf);
                attroff(COLOR_PAIR(dur_pair) | A_NORMAL);
            }

            if (is_active) {
                int adot_visible = (timer->state == TIMER_PAUSED) || ((anim_tick % 10) < 7);
                if (adot_visible) {
                    int mark_pair;
                    if (is_sel) mark_pair = (timer->state == TIMER_RUNNING) ? CP_SELECT_RUN : CP_SELECT_PAUSE;
                    else mark_pair = (timer->state == TIMER_RUNNING) ? CP_RUNNING : CP_PAUSED;
                    attron(COLOR_PAIR(mark_pair) | A_BOLD);
                    mvprintw(row, panel_x + panel_w - 3, "\xe2\x97\x8f");
                    attroff(COLOR_PAIR(mark_pair) | A_BOLD);
                }
            }

            if (is_sel) {
                int hint_pair = CP_SELECT_ACCENT;
                attron(COLOR_PAIR(hint_pair) | A_BOLD);
                mvprintw(row, panel_x + panel_w - 17, "[Enter=Start]");
                attroff(COLOR_PAIR(hint_pair) | A_BOLD);
            }
        }
        if (start > 0) mvprintw(list_top - 1, panel_x + panel_w - 10, "\xe2\x86\x91 more");
        if (end < task_count) mvprintw(list_top + visible_rows, panel_x + panel_w - 10, "\xe2\x86\x93 more");
    }

    const char *task_hint = (focus == FOCUS_TASKS)
        ? "[\xe2\x86\x91\xe2\x86\x93] move  [Enter] start  [a] add  [Space] done  [d] delete"
        : "[Tab] switch focus to tasks";
    put_centered(tasks_y + tasks_h - 2, panel_x + 2, panel_w - 4, task_hint, CP_DIM, A_NORMAL);

    /* ---- stats strip ---- */
    int stats_y = LINES - 3;
    char today_buf[32];
    dt_format_human(today_total, today_buf, sizeof(today_buf));

    const char *levels = "\xe2\x96\x81\xe2\x96\x82\xe2\x96\x83\xe2\x96\x84\xe2\x96\x85\xe2\x96\x86\xe2\x96\x87\xe2\x96\x88";
    /* levels holds 8 three-byte UTF-8 codepoints back to back; index safely by decoding */
    const char *level_ptrs[8];
    for (int i = 0; i < 8; i++) level_ptrs[i] = levels + i * 3;

    long maxv = 0;
    for (int i = 0; i < 7; i++) if (week_totals[i] > maxv) maxv = week_totals[i];

    char spark[8][4];
    for (int i = 0; i < 7; i++) {
        int lvl = 0;
        if (maxv > 0) {
            lvl = (int)((week_totals[i] / (double)maxv) * 7.0 + 0.5);
            if (lvl > 7) lvl = 7;
        }
        memcpy(spark[i], level_ptrs[lvl], 3);
        spark[i][3] = '\0';
    }

    char statsline[128];
    snprintf(statsline, sizeof(statsline), "Today: %s tracked    Last 7 days: %s%s%s%s%s%s%s",
              today_buf, spark[0], spark[1], spark[2], spark[3], spark[4], spark[5], spark[6]);
    put_centered(stats_y, 0, width, statsline, CP_ACCENT, A_NORMAL);

    draw_toast(stats_y + 1, width, toast);

    /* ---- footer ---- */
    attron(COLOR_PAIR(CP_FOOTER));
    mvprintw(LINES - 1, 0, "%*s", width, "");
    mvprintw(LINES - 1, 2, " [Tab] focus   [c] calendar   [q] quit ");
    attroff(COLOR_PAIR(CP_FOOTER));

    refresh();
}

/* ---------- calendar screen ---------- */

static int heat_pair_for(long seconds) {
    if (seconds <= 0) return CP_HEAT0;
    if (seconds < 1800) return CP_HEAT1;
    if (seconds < 3600) return CP_HEAT2;
    if (seconds < 7200) return CP_HEAT3;
    return CP_HEAT4;
}

void ui_draw_calendar(int year, int month, const long totals[32], int sel_day, const char *toast) {
    erase();
    if (screen_too_small()) return;

    int width = COLS;
    char title[64];
    snprintf(title, sizeof(title), "\xe2\x80\xb9  %s %d  \xe2\x80\xba", dt_month_name(month), year);
    put_centered(1, 0, width, title, CP_TITLE, A_BOLD);
    hline_full(2, 2, width - 4, CP_BORDER_DIM);

    int cell_w = 6;
    int grid_w = cell_w * 7;
    int grid_x = (width - grid_w) / 2;
    int header_y = 4;

    attron(COLOR_PAIR(CP_DIM) | A_BOLD);
    for (int d = 0; d < 7; d++) {
        mvprintw(header_y, grid_x + d * cell_w + 2, "%s", dt_weekday_short(d));
    }
    attroff(COLOR_PAIR(CP_DIM) | A_BOLD);

    int first_wd = dt_weekday_of(year, month, 1);
    int ndays = dt_days_in_month(year, month);
    char today[11];
    dt_today_str(today);

    int row = 0, col = first_wd;
    int grid_y = header_y + 2;
    for (int day = 1; day <= ndays; day++) {
        int cy = grid_y + row * 3;
        int cx = grid_x + col * cell_w;
        int pair = heat_pair_for(totals[day]);
        char datebuf[11];
        snprintf(datebuf, sizeof(datebuf), "%04d-%02d-%02d", year, month, day);
        int is_today = (strcmp(datebuf, today) == 0);
        int is_sel = (day == sel_day);

        attron(COLOR_PAIR(pair) | (is_sel ? A_BOLD | A_REVERSE : A_NORMAL));
        mvprintw(cy, cx, "      ");
        mvprintw(cy + 1, cx, "  %2d  ", day);
        mvprintw(cy + 2, cx, "      ");
        attroff(COLOR_PAIR(pair) | (is_sel ? A_BOLD | A_REVERSE : A_NORMAL));

        if (is_today) {
            attron(COLOR_PAIR(CP_ACCENT) | A_BOLD);
            mvprintw(cy + 1, cx, "\xe2\x97\x86");
            attroff(COLOR_PAIR(CP_ACCENT) | A_BOLD);
        }
        /* Selection is shown by reversing the cell's own colors above;
         * no separate marker needed (and one drawn in the cell margin
         * would get clobbered by the next day's fill on the same row). */

        col++;
        if (col > 6) { col = 0; row++; }
    }

    int legend_y = grid_y + (row + 1) * 3 + 1;
    int lx = (width - 40) / 2;
    attron(COLOR_PAIR(CP_DIM));
    mvprintw(legend_y, lx, "Less");
    attroff(COLOR_PAIR(CP_DIM));
    int swatches[5] = {CP_HEAT0, CP_HEAT1, CP_HEAT2, CP_HEAT3, CP_HEAT4};
    for (int i = 0; i < 5; i++) {
        attron(COLOR_PAIR(swatches[i]));
        mvprintw(legend_y, lx + 6 + i * 3, "  ");
        attroff(COLOR_PAIR(swatches[i]));
    }
    attron(COLOR_PAIR(CP_DIM));
    mvprintw(legend_y, lx + 6 + 5 * 3 + 1, "More");
    attroff(COLOR_PAIR(CP_DIM));

    draw_toast(LINES - 3, width, toast);

    attron(COLOR_PAIR(CP_FOOTER));
    mvprintw(LINES - 1, 0, "%*s", width, "");
    mvprintw(LINES - 1, 2, " [\xe2\x86\x90\xe2\x86\x92\xe2\x86\x91\xe2\x86\x93] move   [PgUp/PgDn] month   [Enter] view day   [Esc] back ");
    attroff(COLOR_PAIR(CP_FOOTER));

    refresh();
}

/* ---------- day detail screen ---------- */

void ui_draw_day_detail(const char *date, const session_t *sessions, int count,
                         int sel, int tasks_done, int tasks_total, const char *toast) {
    erase();
    if (screen_too_small()) return;

    int width = COLS;
    int y, m, d;
    sscanf(date, "%d-%d-%d", &y, &m, &d);
    int wd = dt_weekday_of(y, m, d);
    static const char *WEEKDAY_LONG[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

    char title[80];
    snprintf(title, sizeof(title), "%s, %s %d %d", WEEKDAY_LONG[wd], dt_month_name(m), d, y);
    put_centered(1, 0, width, title, CP_TITLE, A_BOLD);

    long total = 0;
    for (int i = 0; i < count; i++) total += sessions[i].elapsed;
    char totalbuf[32];
    dt_format_human(total, totalbuf, sizeof(totalbuf));
    char subtitle[128];
    if (tasks_total > 0)
        snprintf(subtitle, sizeof(subtitle), "%d session%s \xc2\xb7 %s tracked \xc2\xb7 %d/%d tasks done",
                 count, count == 1 ? "" : "s", totalbuf, tasks_done, tasks_total);
    else
        snprintf(subtitle, sizeof(subtitle), "%d session%s \xc2\xb7 %s tracked",
                 count, count == 1 ? "" : "s", totalbuf);
    put_centered(2, 0, width, subtitle, CP_DIM, A_NORMAL);

    hline_full(3, 2, width - 4, CP_BORDER_DIM);

    int panel_w = width - 4 < 70 ? width - 4 : 70;
    int panel_x = (width - panel_w) / 2;
    int list_y = 5;

    if (count == 0) {
        put_centered(list_y + 1, panel_x, panel_w, "No sessions logged this day.", CP_DIM, A_NORMAL);
    } else {
        /* Fixed columns: "HH:MM:SS - HH:MM:SS" (21 incl. spaces/dash) + gaps (4) + duration, reserved generously (10). */
        int task_w = (panel_w - 2) - 21 - 4 - 10;
        if (task_w < 8) task_w = 8;

        for (int i = 0; i < count && list_y + i < LINES - 3; i++) {
            char taskbuf[200];
            truncate_str(sessions[i].task, taskbuf, (size_t)task_w + 1 < sizeof(taskbuf) ? (size_t)task_w + 1 : sizeof(taskbuf));
            char durbuf[32];
            dt_format_human(sessions[i].elapsed, durbuf, sizeof(durbuf));

            char line[300];
            snprintf(line, sizeof(line), "%s \xe2\x80\x93 %s   %-*s %8s",
                     sessions[i].start_hms, sessions[i].end_hms, task_w, taskbuf, durbuf);

            int is_sel = (i == sel);
            int pair = is_sel ? CP_SELECT : CP_TEXT;
            if (is_sel) {
                attron(COLOR_PAIR(CP_SELECT));
                mvprintw(list_y + i, panel_x, "%*s", panel_w, "");
                attroff(COLOR_PAIR(CP_SELECT));
            }
            attron(COLOR_PAIR(pair));
            mvprintw(list_y + i, panel_x + 1, "%.*s", panel_w - 2, line);
            attroff(COLOR_PAIR(pair));
        }
    }

    draw_toast(LINES - 3, width, toast);

    attron(COLOR_PAIR(CP_FOOTER));
    mvprintw(LINES - 1, 0, "%*s", width, "");
    mvprintw(LINES - 1, 2, " [\xe2\x86\x91\xe2\x86\x93] select   [d] delete session   [Esc] back ");
    attroff(COLOR_PAIR(CP_FOOTER));

    refresh();
}

/* ---------- modals ---------- */

static void modal_box(int h, int w, int *out_y, int *out_x) {
    int y = (LINES - h) / 2;
    int x = (COLS - w) / 2;
    draw_box(y, x, h, w, CP_TITLE, A_BOLD);
    for (int i = 1; i < h - 1; i++) {
        attron(COLOR_PAIR(CP_MODAL));
        mvprintw(y + i, x + 1, "%*s", w - 2, "");
        attroff(COLOR_PAIR(CP_MODAL));
    }
    *out_y = y;
    *out_x = x;
}

int ui_modal_text(const char *title, const char *initial, char *buf, size_t bufsize) {
    timeout(-1); /* block for real keystrokes while the modal is up */
    int w = COLS - 10 < 60 ? COLS - 10 : 60;
    if (w < 30) w = COLS - 4;
    int h = 6;
    int y, x;

    char work[512];
    snprintf(work, sizeof(work), "%s", initial ? initial : "");
    int len = (int)strlen(work);
    int cursor = len;

    /* Drawn once: the modal overlays whatever the caller already rendered this frame. */
    modal_box(h, w, &y, &x);
    put_centered(y + 1, x, w, title, CP_TITLE, A_BOLD);
    put_centered(y + h - 2, x, w, "[Enter] confirm   [Esc] cancel", CP_DIM, A_NORMAL);

    curs_set(1);
    int confirmed = 0;
    for (;;) {
        attron(COLOR_PAIR(CP_MODAL));
        mvprintw(y + 3, x + 2, "> %-*s", w - 4, work);
        attroff(COLOR_PAIR(CP_MODAL));
        move(y + 3, x + 4 + cursor);
        refresh();

        int ch = getch();
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            confirmed = 1;
            break;
        } else if (ch == 27) {
            confirmed = 0;
            break;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (cursor > 0) {
                memmove(work + cursor - 1, work + cursor, len - cursor + 1);
                cursor--;
                len--;
            }
        } else if (ch == KEY_LEFT) {
            if (cursor > 0) cursor--;
        } else if (ch == KEY_RIGHT) {
            if (cursor < len) cursor++;
        } else if (ch >= 32 && ch < 127) {
            if (len < (int)sizeof(work) - 1 && len < w - 5) {
                memmove(work + cursor + 1, work + cursor, len - cursor + 1);
                work[cursor] = (char)ch;
                cursor++;
                len++;
            }
        }
    }
    curs_set(0);

    /* trim leading/trailing spaces */
    char *start = work;
    while (*start == ' ') start++;
    char *end = start + strlen(start);
    while (end > start && *(end - 1) == ' ') end--;
    *end = '\0';

    snprintf(buf, bufsize, "%s", start);
    return confirmed;
}

int ui_modal_confirm(const char *message) {
    timeout(-1);
    int w = (int)strlen(message) + 10;
    if (w > COLS - 4) w = COLS - 4;
    if (w < 30) w = 30;
    int h = 5;
    int y, x;
    modal_box(h, w, &y, &x);
    put_centered(y + 1, x, w, message, CP_TITLE, A_BOLD);
    put_centered(y + 3, x, w, "[y] yes    [n] no", CP_DIM, A_NORMAL);
    refresh();

    for (;;) {
        int ch = getch();
        if (ch == 'y' || ch == 'Y') return 1;
        if (ch == 'n' || ch == 'N' || ch == 27) return 0;
    }
}

int ui_modal_choice(const char *title, const char *message, const char *labels[],
                     const char keys[], int n) {
    timeout(-1);
    int w = COLS - 10 < 60 ? COLS - 10 : 60;
    if (w < 30) w = COLS - 4;
    int h = 6 + n;
    int y, x;
    modal_box(h, w, &y, &x);
    put_centered(y + 1, x, w, title, CP_TITLE, A_BOLD);
    put_centered(y + 2, x, w, message, CP_DIM, A_NORMAL);

    for (int i = 0; i < n; i++) {
        char line[128];
        snprintf(line, sizeof(line), "[%c] %s", keys[i], labels[i]);
        put_centered(y + 4 + i, x, w, line, CP_TEXT, A_NORMAL);
    }
    refresh();

    for (;;) {
        int ch = getch();
        if (ch == 27) return -1;
        for (int i = 0; i < n; i++) {
            if (tolower(ch) == tolower((unsigned char)keys[i])) return i;
        }
    }
}
