#ifndef QUIR_BIGFONT_H
#define QUIR_BIGFONT_H

#include <curses.h>

#define BIGFONT_ROWS 5 /* rendered (scaled) height */

/* Returns the total rendered width (columns) of `str`, which may contain
 * digits, ':' and ' '. Useful for centering before drawing. */
int bigfont_width(const char *str);

/* Draws `str` at (y, x) in `win` using color pair `pair` (with `attrs` ORed in). */
void bigfont_draw(WINDOW *win, int y, int x, const char *str, int pair, attr_t attrs);

#endif
