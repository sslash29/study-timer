#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define INITIAL_CAP 64

static int get_data_dir(char *out, size_t outsz) {
    const char *home = getenv("HOME");
    if (!home || !*home) return -1;
    snprintf(out, outsz, "%s/.quir", home);
    return 0;
}

int storage_init(void) {
    char dir[768];
    if (get_data_dir(dir, sizeof(dir)) != 0) return -1;
    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0) return -1;
    }
    return 0;
}

static int get_path(const char *filename, char *out, size_t outsz) {
    char dir[768];
    if (get_data_dir(dir, sizeof(dir)) != 0) return -1;
    snprintf(out, outsz, "%s/%s", dir, filename);
    return 0;
}

static void sanitize_field(char *s) {
    for (; *s; s++) {
        if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
    }
}

/* ---- generic whole-file line I/O ---- */

static char **read_lines(const char *path, long *count) {
    *count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    size_t cap = INITIAL_CAP;
    char **lines = malloc(cap * sizeof(char *));
    if (!lines) { fclose(f); return NULL; }

    char *buf = NULL;
    size_t bufcap = 0;
    ssize_t len;
    long n = 0;
    while ((len = getline(&buf, &bufcap, f)) != -1) {
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        if ((size_t)n >= cap) {
            cap *= 2;
            char **grown = realloc(lines, cap * sizeof(char *));
            if (!grown) break;
            lines = grown;
        }
        lines[n] = strdup(buf);
        n++;
    }
    free(buf);
    fclose(f);
    *count = n;
    return lines;
}

static void free_lines(char **lines, long count) {
    if (!lines) return;
    for (long i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

static int write_lines(const char *path, char **lines, long count) {
    char tmp[800];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    for (long i = 0; i < count; i++) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

static int append_line(const char *path, const char *line) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", line);
    fclose(f);
    return 0;
}

/* Rewrites the file with the line at line_no removed. */
static int remove_line_at(const char *path, long line_no) {
    long count;
    char **lines = read_lines(path, &count);
    if (!lines || line_no < 0 || line_no >= count) {
        free_lines(lines, count);
        return -1;
    }
    free(lines[line_no]);
    int shifted = (line_no < count - 1);
    for (long i = line_no; i < count - 1; i++) lines[i] = lines[i + 1];
    int rc = write_lines(path, lines, count - 1);
    if (shifted) free(lines[count - 1]); /* dangling duplicate left after the shift */
    free(lines);
    return rc;
}

static int replace_line_at(const char *path, long line_no, const char *new_line) {
    long count;
    char **lines = read_lines(path, &count);
    if (!lines || line_no < 0 || line_no >= count) {
        free_lines(lines, count);
        return -1;
    }
    free(lines[line_no]);
    lines[line_no] = strdup(new_line);
    int rc = write_lines(path, lines, count);
    free_lines(lines, count);
    return rc;
}

/* ---- Sessions ---- */

int storage_append_session(const char *date, const char *start_hms, const char *end_hms,
                            long elapsed, const char *task) {
    if (storage_init() != 0) return -1;
    char path[768];
    if (get_path("sessions.log", path, sizeof(path)) != 0) return -1;

    char clean_task[TASK_TEXT_MAX];
    snprintf(clean_task, sizeof(clean_task), "%s", task);
    sanitize_field(clean_task);

    char line[700];
    snprintf(line, sizeof(line), "%s\t%s\t%s\t%ld\t%s", date, start_hms, end_hms, elapsed, clean_task);
    return append_line(path, line);
}

static int parse_session_line(const char *line, session_t *out) {
    char date[11], start_hms[9], end_hms[9], task[TASK_TEXT_MAX];
    long elapsed;
    /* fields are tab separated; task may contain spaces but not tabs */
    const char *p = line;
    const char *tabs[4];
    int t = 0;
    for (const char *c = line; *c && t < 4; c++) {
        if (*c == '\t') tabs[t++] = c;
    }
    if (t < 4) return -1;

    size_t l0 = (size_t)(tabs[0] - p);
    size_t l1 = (size_t)(tabs[1] - tabs[0] - 1);
    size_t l2 = (size_t)(tabs[2] - tabs[1] - 1);
    size_t l3 = (size_t)(tabs[3] - tabs[2] - 1);
    if (l0 >= sizeof(date) || l1 >= sizeof(start_hms) || l2 >= sizeof(end_hms) || l3 >= 32) return -1;

    memcpy(date, p, l0); date[l0] = '\0';
    memcpy(start_hms, tabs[0] + 1, l1); start_hms[l1] = '\0';
    memcpy(end_hms, tabs[1] + 1, l2); end_hms[l2] = '\0';

    char elapsed_buf[32];
    memcpy(elapsed_buf, tabs[2] + 1, l3); elapsed_buf[l3] = '\0';
    elapsed = atol(elapsed_buf);

    snprintf(task, sizeof(task), "%s", tabs[3] + 1);

    snprintf(out->date, sizeof(out->date), "%s", date);
    snprintf(out->start_hms, sizeof(out->start_hms), "%s", start_hms);
    snprintf(out->end_hms, sizeof(out->end_hms), "%s", end_hms);
    out->elapsed = elapsed;
    snprintf(out->task, sizeof(out->task), "%s", task);
    return 0;
}

session_t *storage_get_sessions_for_date(const char *date, int *count) {
    *count = 0;
    char path[768];
    if (get_path("sessions.log", path, sizeof(path)) != 0) return NULL;
    long n;
    char **lines = read_lines(path, &n);
    if (!lines) return NULL;

    session_t *out = malloc((size_t)(n > 0 ? n : 1) * sizeof(session_t));
    int found = 0;
    for (long i = 0; i < n; i++) {
        session_t s;
        if (parse_session_line(lines[i], &s) == 0 && strcmp(s.date, date) == 0) {
            s.line_no = i;
            out[found++] = s;
        }
    }
    free_lines(lines, n);
    *count = found;
    if (found == 0) { free(out); return NULL; }
    return out;
}

int storage_delete_session_line(long line_no) {
    char path[768];
    if (get_path("sessions.log", path, sizeof(path)) != 0) return -1;
    return remove_line_at(path, line_no);
}

void storage_get_month_totals(int year, int month, long totals[32]) {
    for (int i = 0; i < 32; i++) totals[i] = 0;
    char path[768];
    if (get_path("sessions.log", path, sizeof(path)) != 0) return;
    long n;
    char **lines = read_lines(path, &n);
    if (!lines) return;

    char prefix[9];
    snprintf(prefix, sizeof(prefix), "%04d-%02d-", year, month);

    for (long i = 0; i < n; i++) {
        session_t s;
        if (parse_session_line(lines[i], &s) != 0) continue;
        if (strncmp(s.date, prefix, 8) != 0) continue;
        int day = atoi(s.date + 8);
        if (day >= 1 && day <= 31) totals[day] += s.elapsed;
    }
    free_lines(lines, n);
}

long storage_get_day_total(const char *date) {
    int count;
    session_t *sessions = storage_get_sessions_for_date(date, &count);
    long total = 0;
    for (int i = 0; i < count; i++) total += sessions[i].elapsed;
    free(sessions);
    return total;
}

void storage_get_totals_for_dates(const char dates[][11], int n, long *totals) {
    for (int i = 0; i < n; i++) totals[i] = 0;
    char path[768];
    if (get_path("sessions.log", path, sizeof(path)) != 0) return;
    long linecount;
    char **lines = read_lines(path, &linecount);
    if (!lines) return;

    for (long i = 0; i < linecount; i++) {
        session_t s;
        if (parse_session_line(lines[i], &s) != 0) continue;
        for (int d = 0; d < n; d++) {
            if (strcmp(s.date, dates[d]) == 0) {
                totals[d] += s.elapsed;
                break;
            }
        }
    }
    free_lines(lines, linecount);
}

/* ---- Tasks ---- */

static int parse_task_line(const char *line, task_t *out) {
    /* date\tdone\ttext */
    const char *p = line;
    const char *tab1 = strchr(p, '\t');
    if (!tab1) return -1;
    const char *tab2 = strchr(tab1 + 1, '\t');
    if (!tab2) return -1;

    size_t l0 = (size_t)(tab1 - p);
    size_t l1 = (size_t)(tab2 - tab1 - 1);
    if (l0 >= sizeof(out->date) || l1 >= 8) return -1;

    memcpy(out->date, p, l0); out->date[l0] = '\0';
    char donebuf[8];
    memcpy(donebuf, tab1 + 1, l1); donebuf[l1] = '\0';
    out->done = atoi(donebuf);
    snprintf(out->text, sizeof(out->text), "%s", tab2 + 1);
    return 0;
}

task_t *storage_get_tasks_for_date(const char *date, int *count) {
    *count = 0;
    char path[768];
    if (get_path("tasks.log", path, sizeof(path)) != 0) return NULL;
    long n;
    char **lines = read_lines(path, &n);
    if (!lines) return NULL;

    task_t *out = malloc((size_t)(n > 0 ? n : 1) * sizeof(task_t));
    int found = 0;
    for (long i = 0; i < n; i++) {
        task_t t;
        if (parse_task_line(lines[i], &t) == 0 && strcmp(t.date, date) == 0) {
            t.line_no = i;
            out[found++] = t;
        }
    }
    free_lines(lines, n);
    *count = found;
    if (found == 0) { free(out); return NULL; }
    return out;
}

int storage_add_task(const char *date, const char *text) {
    if (storage_init() != 0) return -1;
    char path[768];
    if (get_path("tasks.log", path, sizeof(path)) != 0) return -1;

    char clean[TASK_TEXT_MAX];
    snprintf(clean, sizeof(clean), "%s", text);
    sanitize_field(clean);

    char line[TASK_TEXT_MAX + 32];
    snprintf(line, sizeof(line), "%s\t0\t%s", date, clean);
    return append_line(path, line);
}

int storage_toggle_task(long line_no) {
    char path[768];
    if (get_path("tasks.log", path, sizeof(path)) != 0) return -1;
    long n;
    char **lines = read_lines(path, &n);
    if (!lines || line_no < 0 || line_no >= n) { free_lines(lines, n); return -1; }

    task_t t;
    if (parse_task_line(lines[line_no], &t) != 0) { free_lines(lines, n); return -1; }
    t.done = !t.done;

    char newline[TASK_TEXT_MAX + 32];
    snprintf(newline, sizeof(newline), "%s\t%d\t%s", t.date, t.done, t.text);
    free(lines[line_no]);
    lines[line_no] = strdup(newline);
    int rc = write_lines(path, lines, n);
    free_lines(lines, n);
    return rc;
}

int storage_delete_task_line(long line_no) {
    char path[768];
    if (get_path("tasks.log", path, sizeof(path)) != 0) return -1;
    return remove_line_at(path, line_no);
}

int storage_edit_task_line(long line_no, const char *new_text) {
    char path[768];
    if (get_path("tasks.log", path, sizeof(path)) != 0) return -1;
    long n;
    char **lines = read_lines(path, &n);
    if (!lines || line_no < 0 || line_no >= n) { free_lines(lines, n); return -1; }

    task_t t;
    if (parse_task_line(lines[line_no], &t) != 0) { free_lines(lines, n); return -1; }

    char clean[TASK_TEXT_MAX];
    snprintf(clean, sizeof(clean), "%s", new_text);
    sanitize_field(clean);

    char newline[TASK_TEXT_MAX + 32];
    snprintf(newline, sizeof(newline), "%s\t%d\t%s", t.date, t.done, clean);
    free_lines(lines, n);

    return replace_line_at(path, line_no, newline);
}

/* ---- current.state ---- */

int storage_state_save(const state_snapshot_t *snap) {
    if (storage_init() != 0) return -1;
    char path[768];
    if (get_path("current.state", path, sizeof(path)) != 0) return -1;

    char clean_task[TASK_TEXT_MAX];
    snprintf(clean_task, sizeof(clean_task), "%s", snap->task);
    sanitize_field(clean_task);

    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "task=%s\n", clean_task);
    fprintf(f, "start_epoch=%ld\n", (long)snap->start_epoch);
    fprintf(f, "elapsed_base=%ld\n", snap->elapsed_base);
    fprintf(f, "segment_start=%ld\n", (long)snap->segment_start);
    fprintf(f, "is_paused=%d\n", snap->is_paused);
    fclose(f);
    return 0;
}

int storage_state_load(state_snapshot_t *snap) {
    memset(snap, 0, sizeof(*snap));
    char path[768];
    if (get_path("current.state", path, sizeof(path)) != 0) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r')) val[--vlen] = '\0';

        if (strcmp(key, "task") == 0) snprintf(snap->task, sizeof(snap->task), "%s", val);
        else if (strcmp(key, "start_epoch") == 0) snap->start_epoch = (time_t)atol(val);
        else if (strcmp(key, "elapsed_base") == 0) snap->elapsed_base = atol(val);
        else if (strcmp(key, "segment_start") == 0) snap->segment_start = (time_t)atol(val);
        else if (strcmp(key, "is_paused") == 0) snap->is_paused = atoi(val);
    }
    fclose(f);
    snap->valid = (snap->task[0] != '\0');
    return snap->valid;
}

int storage_state_clear(void) {
    char path[768];
    if (get_path("current.state", path, sizeof(path)) != 0) return -1;
    if (access(path, F_OK) != 0) return 0;
    return remove(path);
}
