#include "bigfont.h"
#include <string.h>

#define GLYPH_ROWS 5
#define GAP 1 /* columns between rendered characters */

typedef struct {
    const char *rows[GLYPH_ROWS];
    int width; /* in unscaled cells */
} glyph_t;

static const glyph_t DIGITS[10] = {
    { {".##.", "#..#", "#..#", "#..#", ".##."}, 4 }, /* 0 */
    { {"..#.", ".##.", "..#.", "..#.", ".###"}, 4 }, /* 1 */
    { {"###.", "...#", ".##.", "#...", "####"}, 4 }, /* 2 */
    { {"###.", "...#", ".##.", "...#", "###."}, 4 }, /* 3 */
    { {"#..#", "#..#", "####", "...#", "...#"}, 4 }, /* 4 */
    { {"####", "#...", "###.", "...#", "###."}, 4 }, /* 5 */
    { {".##.", "#...", "###.", "#..#", ".##."}, 4 }, /* 6 */
    { {"####", "...#", "..#.", ".#..", ".#.."}, 4 }, /* 7 */
    { {".##.", "#..#", ".##.", "#..#", ".##."}, 4 }, /* 8 */
    { {".##.", "#..#", ".###", "...#", ".##."}, 4 }, /* 9 */
};

static const glyph_t COLON = { {"..", "##", "..", "##", ".."}, 2 };
static const glyph_t SPACE = { {"..", "..", "..", "..", ".."}, 2 };

static const glyph_t *glyph_for(char c) {
    if (c >= '0' && c <= '9') return &DIGITS[c - '0'];
    if (c == ':') return &COLON;
    return &SPACE;
}

int bigfont_width(const char *str) {
    int total = 0;
    int n = (int)strlen(str);
    for (int i = 0; i < n; i++) {
        const glyph_t *g = glyph_for(str[i]);
        total += g->width * 2;
        if (i < n - 1) total += GAP;
    }
    return total;
}

void bigfont_draw(WINDOW *win, int y, int x, const char *str, int pair, attr_t attrs) {
    int n = (int)strlen(str);
    int cursor_x = x;

    for (int i = 0; i < n; i++) {
        const glyph_t *g = glyph_for(str[i]);
        for (int r = 0; r < GLYPH_ROWS; r++) {
            {
                int out_y = y + r;
                int cx = cursor_x;
                for (int c = 0; c < g->width; c++) {
                    if (g->rows[r][c] == '#') {
                        wattron(win, COLOR_PAIR(pair) | attrs | A_REVERSE);
                        mvwprintw(win, out_y, cx, "  ");
                        wattroff(win, COLOR_PAIR(pair) | attrs | A_REVERSE);
                    }
                    cx += 2;
                }
            }
        }
        cursor_x += g->width * 2 + GAP;
    }
}
