#ifndef MINUTA_APP_H
#define MINUTA_APP_H

#include <windows.h>

#include "document.h"

#define APP_NAME "Minuta"

#define COLUMN_MIN 50
#define COLUMN_MAX 80
#define FONT_SIZE_MIN 8
#define FONT_SIZE_MAX 28

struct settings {
    char font_name[LF_FACESIZE];
    int font_size;       /* points */
    int column_width;    /* characters, COLUMN_MIN..COLUMN_MAX */
    int night;
    int focus;
    int focus_paragraph; /* focus scope: 1 paragraph, 0 sentence */
    int typewriter;
    char last_file[MAX_PATH];
    RECT window;         /* normal (restored) position */
    int maximized;
    int trace;           /* write TRACE.TXT (diagnostics) */
};

struct theme {
    COLORREF text;
    COLORREF background;
    COLORREF marker;          /* dimmed Markdown syntax */
    COLORREF code_background;
    COLORREF faded;           /* text outside the focus */
    COLORREF link;
};

struct app {
    HINSTANCE instance;
    HWND window;
    HWND editor;
    HWND preview;
    HACCEL accelerators;
    struct settings settings;
    struct theme theme;
    char path[MAX_PATH];     /* empty for an untitled document */
    struct document_format format;
    int modified;
    int previewing;
    int fullscreen;
};

extern struct app app;

/* settings.c */
void settings_load(struct settings *settings);
void settings_save(const struct settings *settings);
/* Directory of MINUTA.EXE with a trailing backslash. */
void app_directory(char *path, size_t size);

/* main.c */
void app_update_title(void);
void app_theme_update(void);
void app_error(const char *format, const char *argument);
/* The text changed: mark the document modified. */
void app_document_changed(void);

#endif
