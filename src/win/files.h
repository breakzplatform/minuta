#ifndef MINUTA_FILES_H
#define MINUTA_FILES_H

/* Asks to save a modified document. Returns 1 if the caller may go on
 * (saved or discarded), 0 if the user cancelled. */
int files_confirm_discard(void);

void files_new(void);
void files_open(void);
int files_open_path(const char *path);
int files_save(void);
int files_save_as(void);

/* Writes the crash-safety copy (name.md~) if the document is modified. */
void files_autosave(void);
void files_remove_autosave(void);

/* Reads a whole file (malloc'ed, NUL-terminated). NULL on failure. */
char *files_read(const char *path, size_t *size);
/* Writes through a temporary file so a failed write keeps the old one. */
int files_write(const char *path, const char *data, size_t size);

#endif
