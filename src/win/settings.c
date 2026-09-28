#include <stdio.h>
#include <string.h>

#include "app.h"

#define SECTION "Minuta"

void app_directory(char *path, size_t size)
{
    DWORD length = GetModuleFileName(NULL, path, (DWORD)size);
    char *slash;

    if (length == 0 || length >= size) {
        path[0] = '\0';
        return;
    }
    slash = strrchr(path, '\\');
    if (slash != NULL) {
        slash[1] = '\0';
    } else {
        path[0] = '\0';
    }
}

static void ini_path(char *path, size_t size)
{
    app_directory(path, size);
    if (strlen(path) + sizeof("MINUTA.INI") <= size) {
        strcat(path, "MINUTA.INI");
    }
}

static int clamp(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

static int read_int(const char *ini, const char *key, int fallback)
{
    return (int)GetPrivateProfileInt(SECTION, key, fallback, ini);
}

static void write_int(const char *ini, const char *key, int value)
{
    char text[16];

    sprintf(text, "%d", value);
    WritePrivateProfileString(SECTION, key, text, ini);
}

void settings_load(struct settings *settings)
{
    char ini[MAX_PATH];

    ini_path(ini, sizeof(ini));
    GetPrivateProfileString(SECTION, "Font", "Courier New", settings->font_name,
                            sizeof(settings->font_name), ini);
    settings->font_size = clamp(read_int(ini, "FontSize", 12), FONT_SIZE_MIN, FONT_SIZE_MAX);
    settings->column_width = clamp(read_int(ini, "ColumnWidth", 64), COLUMN_MIN, COLUMN_MAX);
    settings->night = read_int(ini, "Night", 0) != 0;
    settings->focus = read_int(ini, "Focus", 0) != 0;
    settings->focus_paragraph = read_int(ini, "FocusParagraph", 0) != 0;
    settings->typewriter = read_int(ini, "Typewriter", 0) != 0;
    GetPrivateProfileString(SECTION, "LastFile", "", settings->last_file,
                            sizeof(settings->last_file), ini);
    settings->window.left = read_int(ini, "WindowLeft", CW_USEDEFAULT);
    settings->window.top = read_int(ini, "WindowTop", CW_USEDEFAULT);
    settings->window.right = settings->window.left + read_int(ini, "WindowWidth", 640);
    settings->window.bottom = settings->window.top + read_int(ini, "WindowHeight", 520);
    settings->maximized = read_int(ini, "Maximized", 0) != 0;
}

void settings_save(const struct settings *settings)
{
    char ini[MAX_PATH];

    ini_path(ini, sizeof(ini));
    WritePrivateProfileString(SECTION, "Font", settings->font_name, ini);
    write_int(ini, "FontSize", settings->font_size);
    write_int(ini, "ColumnWidth", settings->column_width);
    write_int(ini, "Night", settings->night);
    write_int(ini, "Focus", settings->focus);
    write_int(ini, "FocusParagraph", settings->focus_paragraph);
    write_int(ini, "Typewriter", settings->typewriter);
    WritePrivateProfileString(SECTION, "LastFile", settings->last_file, ini);
    write_int(ini, "WindowLeft", settings->window.left);
    write_int(ini, "WindowTop", settings->window.top);
    write_int(ini, "WindowWidth", settings->window.right - settings->window.left);
    write_int(ini, "WindowHeight", settings->window.bottom - settings->window.top);
    write_int(ini, "Maximized", settings->maximized);
    /* Flush the profile cache so a crash can't lose the file. */
    WritePrivateProfileString(NULL, NULL, NULL, ini);
}
