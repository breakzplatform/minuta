/* RichEdit 2.0 behavior probe: run on the VM, writes C:\PROBE.TXT. */
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

static FILE *out;
static const char *source;
static size_t source_size;
static size_t source_position;

static DWORD CALLBACK read_stream(DWORD_PTR cookie, LPBYTE buffer, LONG wanted, LONG *copied)
{
    size_t count = source_size - source_position;

    (void)cookie;
    if (count > (size_t)wanted) {
        count = (size_t)wanted;
    }
    memcpy(buffer, source + source_position, count);
    source_position += count;
    *copied = (LONG)count;
    return 0;
}

static void dump(const char *label, const char *text, long size)
{
    long i;

    fprintf(out, "%s (%ld):", label, size);
    for (i = 0; i < size; i++) {
        fprintf(out, " %02X", (unsigned char)text[i]);
    }
    fprintf(out, "\n");
}

static int is_bold(HWND editor, long position)
{
    CHARFORMAT2 format;
    CHARRANGE range;

    range.cpMin = position;
    range.cpMax = position + 1;
    SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&range);
    memset(&format, 0, sizeof(format));
    format.cbSize = sizeof(format);
    SendMessage(editor, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    return (format.dwEffects & CFE_BOLD) != 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    HWND editor;
    EDITSTREAM stream;
    GETTEXTLENGTHEX query;
    TEXTRANGE range;
    CHARRANGE selection;
    CHARFORMAT2 format;
    char text[64];
    long count;

    (void)previous;
    (void)command_line;
    (void)show;
    out = fopen("C:\\PROBE.TXT", "w");
    LoadLibrary("RICHED20.DLL");
    editor = CreateWindowEx(0, RICHEDIT_CLASSA, "", WS_POPUP | ES_MULTILINE, 0, 0, 300, 200,
                            NULL, NULL, instance, NULL);
    fprintf(out, "editor %p\n", (void *)editor);

    source = "ab\r\ncd\r\nef";
    source_size = strlen(source);
    stream.dwCookie = 0;
    stream.dwError = 0;
    stream.pfnCallback = read_stream;
    SendMessage(editor, EM_STREAMIN, SF_TEXT, (LPARAM)&stream);

    query.codepage = CP_ACP;
    query.flags = GTL_NUMCHARS;
    fprintf(out, "length numchars %ld\n", (long)SendMessage(editor, EM_GETTEXTLENGTHEX, (WPARAM)&query, 0));
    query.flags = GTL_USECRLF | GTL_NUMCHARS;
    fprintf(out, "length usecrlf %ld\n", (long)SendMessage(editor, EM_GETTEXTLENGTHEX, (WPARAM)&query, 0));
    fprintf(out, "WM_GETTEXTLENGTH %ld\n", (long)SendMessage(editor, WM_GETTEXTLENGTH, 0, 0));

    memset(text, 0, sizeof(text));
    range.chrg.cpMin = 0;
    range.chrg.cpMax = 8;
    range.lpstrText = text;
    count = (long)SendMessage(editor, EM_GETTEXTRANGE, 0, (LPARAM)&range);
    dump("GETTEXTRANGE 0..8", text, count);

    memset(text, 0, sizeof(text));
    range.chrg.cpMin = 3;
    range.chrg.cpMax = 5;
    count = (long)SendMessage(editor, EM_GETTEXTRANGE, 0, (LPARAM)&range);
    dump("GETTEXTRANGE 3..5", text, count);

    memset(text, 0, sizeof(text));
    count = (long)SendMessage(editor, WM_GETTEXT, sizeof(text), (LPARAM)text);
    dump("WM_GETTEXT", text, count);

    fprintf(out, "LINEINDEX 1 = %ld, 2 = %ld\n", (long)SendMessage(editor, EM_LINEINDEX, 1, 0),
            (long)SendMessage(editor, EM_LINEINDEX, 2, 0));

    selection.cpMin = 3;
    selection.cpMax = 3;
    SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
    SendMessage(editor, EM_GETSELTEXT, 0, (LPARAM)text);
    memset(text, 0, sizeof(text));
    selection.cpMin = 2;
    selection.cpMax = 4;
    SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
    count = (long)SendMessage(editor, EM_GETSELTEXT, 0, (LPARAM)text);
    dump("GETSELTEXT 2..4", text, count);

    /* Is a formatting change undoable? */
    SendMessage(editor, EM_EMPTYUNDOBUFFER, 0, 0);
    selection.cpMin = 0;
    selection.cpMax = 0;
    SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
    SendMessage(editor, EM_REPLACESEL, TRUE, (LPARAM)"X");
    fprintf(out, "after typing X: canundo %ld\n", (long)SendMessage(editor, EM_CANUNDO, 0, 0));
    memset(&format, 0, sizeof(format));
    format.cbSize = sizeof(format);
    format.dwMask = CFM_BOLD;
    format.dwEffects = CFE_BOLD;
    selection.cpMin = 4;
    selection.cpMax = 6;
    SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
    SendMessage(editor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    fprintf(out, "bold at 4 after format: %d\n", is_bold(editor, 4));
    SendMessage(editor, EM_UNDO, 0, 0);
    memset(text, 0, sizeof(text));
    range.chrg.cpMin = 0;
    range.chrg.cpMax = 3;
    range.lpstrText = text;
    count = (long)SendMessage(editor, EM_GETTEXTRANGE, 0, (LPARAM)&range);
    dump("text after one undo", text, count);
    fprintf(out, "bold at 4 after one undo: %d (4 is now 'c' or 'd')\n", is_bold(editor, 4));
    fprintf(out, "bold at 3 after one undo: %d\n", is_bold(editor, 3));

    fclose(out);
    return 0;
}
