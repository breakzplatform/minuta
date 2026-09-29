#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <commdlg.h>

#include "app.h"
#include "editor.h"
#include "files.h"
#include "trace.h"

#define FILTER_UTF8 1
#define FILTER_CP1252 2

static const char open_filter[] =
    "Markdown files (*.md;*.markdown;*.txt)\0*.md;*.markdown;*.txt\0"
    "All files (*.*)\0*.*\0";

/* In Save As, the file type chooses the encoding. */
static const char save_filter[] =
    "Markdown, UTF-8 (*.md)\0*.md;*.markdown;*.txt\0"
    "Markdown, Windows-1252 (*.md)\0*.md;*.markdown;*.txt\0";

char *files_read(const char *path, size_t *size)
{
    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD file_size;
    DWORD read = 0;
    char *data;

    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    file_size = GetFileSize(file, NULL);
    data = file_size == 0xFFFFFFFF ? NULL : malloc(file_size + 1);
    if (data != NULL && !ReadFile(file, data, file_size, &read, NULL)) {
        free(data);
        data = NULL;
    }
    CloseHandle(file);
    if (data != NULL) {
        data[read] = '\0';
        *size = read;
    }
    return data;
}

static int write_plain(const char *path, const char *data, size_t size)
{
    HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    BOOL ok;

    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    ok = WriteFile(file, data, (DWORD)size, &written, NULL) && written == size;
    ok = CloseHandle(file) && ok;
    return ok;
}

int files_write(const char *path, const char *data, size_t size)
{
    char temporary[MAX_PATH + 8];

    if (strlen(path) + 5 > sizeof(temporary)) {
        return write_plain(path, data, size);
    }
    strcpy(temporary, path);
    strcat(temporary, ".tmp");
    if (!write_plain(temporary, data, size)) {
        DeleteFile(temporary);
        return 0;
    }
    /* Windows 9x has no MoveFileEx, so replace in two steps. */
    DeleteFile(path);
    if (!MoveFile(temporary, path)) {
        return write_plain(path, data, size) && DeleteFile(temporary);
    }
    return 1;
}

static void autosave_path(char *path, size_t size)
{
    if (app.path[0] != '\0') {
        if (strlen(app.path) + 2 <= size) {
            strcpy(path, app.path);
            strcat(path, "~");
        } else {
            path[0] = '\0';
        }
    } else {
        app_directory(path, size);
        if (strlen(path) + sizeof("UNTITLED.MD~") <= size) {
            strcat(path, "UNTITLED.MD~");
        }
    }
}

void files_remove_autosave(void)
{
    char path[MAX_PATH + 2];

    autosave_path(path, sizeof(path));
    if (path[0] != '\0') {
        DeleteFile(path);
    }
}

static char *encode_document(size_t *size)
{
    size_t text_size;
    char *text = editor_get_text(&text_size);
    char *bytes;

    if (text == NULL) {
        return NULL;
    }
    bytes = document_encode(text, text_size, &app.format, size);
    free(text);
    return bytes;
}

void files_autosave(void)
{
    char path[MAX_PATH + 2];
    char *bytes;
    size_t size;

    if (!app.modified) {
        return;
    }
    autosave_path(path, sizeof(path));
    bytes = path[0] != '\0' ? encode_document(&size) : NULL;
    if (bytes != NULL) {
        write_plain(path, bytes, size);
        free(bytes);
    }
}

static void set_path(const char *path)
{
    strncpy(app.path, path, sizeof(app.path) - 1);
    app.path[sizeof(app.path) - 1] = '\0';
    strcpy(app.settings.last_file, app.path);
}

static void mark_saved(void)
{
    app.modified = 0;
    SendMessage(app.editor, EM_SETMODIFY, FALSE, 0);
    app_update_title();
}

int files_open_path(const char *path)
{
    size_t size;
    size_t text_size;
    char *bytes;
    char *text;

    trace("open %s", path);
    bytes = files_read(path, &size);
    trace("read %lu bytes", (unsigned long)size);
    if (bytes == NULL) {
        app_error("Can't open %s.", path);
        return 0;
    }
    text = document_decode(bytes, size, &app.format, &text_size);
    trace("decoded");
    free(bytes);
    if (text == NULL) {
        app_error("Not enough memory to open %s.", path);
        return 0;
    }
    files_remove_autosave();
    editor_set_text(text, text_size);
    free(text);
    set_path(path);
    mark_saved();
    return 1;
}

int files_confirm_discard(void)
{
    char message[MAX_PATH + 64];
    const char *name;
    int answer;

    if (!app.modified) {
        return 1;
    }
    name = app.path[0] != '\0' ? app.path : "Untitled";
    wsprintf(message, "Save changes to %s?", name);
    answer = MessageBox(app.window, message, APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL) {
        return 0;
    }
    if (answer == IDYES) {
        return files_save();
    }
    files_remove_autosave();
    return 1;
}

void files_new(void)
{
    if (!files_confirm_discard()) {
        return;
    }
    files_remove_autosave();
    document_format_free(&app.format);
    editor_set_text("", 0);
    app.path[0] = '\0';
    mark_saved();
}

void files_open(void)
{
    OPENFILENAME dialog;
    char path[MAX_PATH] = "";

    if (!files_confirm_discard()) {
        return;
    }
    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = open_filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileName(&dialog)) {
        files_open_path(path);
    }
}

static int save_to(const char *path)
{
    size_t size;
    char *bytes = encode_document(&size);
    int ok;

    if (bytes == NULL) {
        app_error("Not enough memory to save %s.", path);
        return 0;
    }
    ok = files_write(path, bytes, size);
    free(bytes);
    if (!ok) {
        app_error("Can't save %s.", path);
        return 0;
    }
    files_remove_autosave();
    set_path(path);
    mark_saved();
    return 1;
}

int files_save(void)
{
    if (app.path[0] == '\0') {
        return files_save_as();
    }
    return save_to(app.path);
}

int files_save_as(void)
{
    OPENFILENAME dialog;
    char path[MAX_PATH];

    strcpy(path, app.path);
    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpstrFilter = save_filter;
    dialog.nFilterIndex = app.format.encoding == ENCODING_CP1252 ? FILTER_CP1252 : FILTER_UTF8;
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrDefExt = "md";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileName(&dialog)) {
        return 0;
    }
    if (dialog.nFilterIndex == FILTER_CP1252 && app.format.encoding != ENCODING_CP1252) {
        /* Characters outside CP1252 can't be kept in a CP1252 file. */
        enum line_ending line_ending = app.format.line_ending;

        document_format_free(&app.format);
        app.format.encoding = ENCODING_CP1252;
        app.format.line_ending = line_ending;
    } else if (dialog.nFilterIndex == FILTER_UTF8) {
        app.format.encoding = ENCODING_UTF8;
    }
    files_remove_autosave();
    return save_to(path);
}
