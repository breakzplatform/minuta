#ifndef MINUTA_UNDO_H
#define MINUTA_UNDO_H

/* Text-only undo. RichEdit's own undo also records every formatting change
 * made by the syntax styling (checked on Windows 98 with tests/win/probe.c),
 * so it is switched off and edits are recorded here instead. */

enum undo_kind {
    UNDO_TYPING,     /* consecutive characters merge, word by word */
    UNDO_BACKSPACE,  /* consecutive backspaces merge */
    UNDO_DELETE,     /* consecutive deletes merge */
    UNDO_OTHER
};

void undo_clear(void);

/* Records that [start, start + removed_size) holding `removed` was replaced
 * by `inserted`. */
void undo_record(long start, const char *removed, long removed_size, const char *inserted,
                 long inserted_size, enum undo_kind kind);

/* Stops merging, so the next edit starts a new undo step. */
void undo_break(void);

int undo_undo(void);
int undo_redo(void);

#endif
