#ifndef MINUTA_TOM_H
#define MINUTA_TOM_H

#include <windows.h>

/* Minimal access to RichEdit's Text Object Model (ITextDocument,
 * ITextRange, ITextFont, ITextPara) from C. Formatting through TOM ranges
 * leaves the selection and the scroll position alone, unlike
 * EM_EXSETSEL + EM_SETCHARFORMAT. The vtable offsets follow tom.h and were
 * checked on Windows 98's RICHED20.DLL with tests/win/probe2.c. */

#define TOM_AUTO_COLOR (-9999997L)

struct tom_font;

/* Returns 1 if the editor exposes TOM. */
int tom_open(HWND editor);
void tom_close(void);

/* A standalone font: the font of [start, start + 1) with these effects.
 * back_color may be TOM_AUTO_COLOR. NULL on failure. */
struct tom_font *tom_font_create(long start, COLORREF color, int bold, int italic, int strike,
                                 long back_color);
void tom_font_free(struct tom_font *font);

void tom_apply_font(long start, long end, struct tom_font *font);

/* 1.5 line spacing and `space_after` points after the paragraph at start. */
void tom_apply_paragraph(long start, float space_after);

#endif
