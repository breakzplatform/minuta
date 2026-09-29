#ifndef MINUTA_STYLE_H
#define MINUTA_STYLE_H

/* Syntax dimming, focus mode and typewriter scrolling for the editor.
 *
 * Styling only ever touches whole paragraphs, and only the ones an edit
 * touched or that are on screen. Work is queued and done when the message
 * queue is empty (style_idle), so typing never waits for it. Fenced code
 * blocks are the only construct spanning paragraphs; a small index of fence
 * lines gives the block state at any paragraph without reading the text
 * above it. */

/* The whole text changed (load, undo of unknown extent, ...). */
void style_reset(void);

/* An edit replaced `removed` characters at `start` with `inserted` ones. */
void style_edit(long start, long removed, long inserted);

/* The view may have scrolled or the caret moved. */
void style_view_changed(void);

/* Theme, font or a mode changed: restyle everything on screen. */
void style_settings_changed(void);

/* Runs queued work; returns 1 if it did something. */
int style_idle(void);

/* Nonzero while styling moves the selection itself; selection-change
 * notifications during that time must be ignored. */
extern int style_busy;

#endif
