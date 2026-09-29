#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "trace.h"

static FILE *log_file;
static DWORD started;

void trace_open(int enabled)
{
    char path[MAX_PATH];

    if (!enabled || log_file != NULL) {
        return;
    }
    app_directory(path, sizeof(path));
    if (strlen(path) + sizeof("TRACE.TXT") <= sizeof(path)) {
        strcat(path, "TRACE.TXT");
        log_file = fopen(path, "w");
    }
    started = GetTickCount();
}

void trace(const char *format, ...)
{
    va_list arguments;

    if (log_file == NULL) {
        return;
    }
    fprintf(log_file, "%6lu ", (unsigned long)(GetTickCount() - started));
    va_start(arguments, format);
    vfprintf(log_file, format, arguments);
    va_end(arguments);
    fputc('\n', log_file);
    fflush(log_file);
}
