/* RichEdit probe 2: scroll position messages and TOM vtable offsets.
 * Writes C:\PROBE2.TXT. */
#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#ifndef EM_GETSCROLLPOS
#define EM_GETSCROLLPOS (WM_USER + 221)
#define EM_SETSCROLLPOS (WM_USER + 222)
#endif

typedef void (*method)(void);
struct object { method *vtable; };
typedef HRESULT (__stdcall *query_function)(void *self, const GUID *iid, void **out);
typedef HRESULT (__stdcall *get_long_function)(void *self, long *value);
typedef HRESULT (__stdcall *set_long_function)(void *self, long value);
typedef HRESULT (__stdcall *range_function)(void *self, long start, long end, void **range);
typedef HRESULT (__stdcall *set_range_function)(void *self, long start, long end);
typedef HRESULT (__stdcall *get_object_function)(void *self, void **object);
typedef HRESULT (__stdcall *set_object_function)(void *self, void *object);
typedef HRESULT (__stdcall *set_float_function)(void *self, float value);
typedef HRESULT (__stdcall *set_spacing_function)(void *self, long rule, float value);
typedef ULONG (__stdcall *release_function)(void *self);

static const GUID iid_text_document = { 0x8CC497C0, 0xA1DF, 0x11CE, { 0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D } };

static char text[200000];

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    FILE *out = fopen("C:\\PROBE2.TXT", "w");
    HWND editor;
    POINT position;
    LRESULT result;
    struct object *ole = NULL;
    struct object *document = NULL;
    struct object *range = NULL;
    struct object *font = NULL;
    long value;
    int i;
    char version[256];
    DWORD handle;

    (void)previous; (void)command_line; (void)show;
    setvbuf(out, NULL, _IONBF, 0);
    LoadLibrary("RICHED20.DLL");
    GetModuleFileName(GetModuleHandle("RICHED20.DLL"), version, sizeof(version));
    fprintf(out, "module %s\n", version);
    fprintf(out, "version info size %lu\n", (unsigned long)GetFileVersionInfoSize(version, &handle));
    editor = CreateWindowEx(0, RICHEDIT_CLASSA, "", WS_POPUP | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE,
                            0, 0, 400, 300, NULL, NULL, instance, NULL);
    text[0] = '\0';
    for (i = 0; i < 1000; i++) {
        char line[32];
        sprintf(line, "line %d\r\n", i);
        strcat(text, line);
    }
    SetWindowText(editor, text);
    SendMessage(editor, EM_LINESCROLL, 0, 500);
    fprintf(out, "first visible after LINESCROLL 500: %ld\n", (long)SendMessage(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    position.x = -1;
    position.y = -1;
    result = SendMessage(editor, EM_GETSCROLLPOS, 0, (LPARAM)&position);
    fprintf(out, "GETSCROLLPOS result %ld point %ld,%ld\n", (long)result, (long)position.x, (long)position.y);
    position.x = 0;
    position.y = 100;
    result = SendMessage(editor, EM_SETSCROLLPOS, 0, (LPARAM)&position);
    fprintf(out, "SETSCROLLPOS 0,100 result %ld first visible %ld\n", (long)result,
            (long)SendMessage(editor, EM_GETFIRSTVISIBLELINE, 0, 0));

    result = SendMessage(editor, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    fprintf(out, "GETOLEINTERFACE %ld %p\n", (long)result, (void *)ole);
    if (ole != NULL) {
        HRESULT hr = ((query_function)ole->vtable[0])(ole, &iid_text_document, (void **)&document);
        fprintf(out, "QI ITextDocument hr %08lX %p\n", (unsigned long)hr, (void *)document);
    }
    if (document != NULL) {
        HRESULT hr;
        value = -1;
        hr = ((get_long_function)document->vtable[9])(document, &value);   /* GetStoryCount */
        fprintf(out, "GetStoryCount hr %08lX value %ld\n", (unsigned long)hr, value);
        hr = ((range_function)document->vtable[24])(document, 5L, 10L, (void **)&range);   /* Range */
        fprintf(out, "Range hr %08lX %p\n", (unsigned long)hr, (void *)range);
    }
    if (range != NULL) {
        HRESULT hr;
        value = -1;
        ((get_long_function)range->vtable[14])(range, &value);   /* GetStart */
        fprintf(out, "GetStart %ld\n", value);
        value = -1;
        ((get_long_function)range->vtable[16])(range, &value);   /* GetEnd */
        fprintf(out, "GetEnd %ld\n", value);
        hr = ((set_range_function)range->vtable[28])(range, 20L, 30L);   /* SetRange */
        value = -1;
        ((get_long_function)range->vtable[14])(range, &value);
        fprintf(out, "after SetRange(20,30) hr %08lX start %ld\n", (unsigned long)hr, value);
        hr = ((get_object_function)range->vtable[18])(range, (void **)&font);   /* GetFont */
        fprintf(out, "GetFont hr %08lX %p\n", (unsigned long)hr, (void *)font);
    }
    if (font != NULL) {
        CHARFORMAT2 format;
        CHARRANGE selection;
        HRESULT hr;
        hr = ((set_long_function)font->vtable[21])(font, -1L);   /* SetBold(tomTrue) */
        fprintf(out, "SetBold hr %08lX\n", (unsigned long)hr);
        hr = ((set_long_function)font->vtable[25])(font, (long)RGB(255, 0, 0));   /* SetForeColor */
        fprintf(out, "SetForeColor hr %08lX\n", (unsigned long)hr);
        hr = ((set_long_function)font->vtable[31])(font, -1L);   /* SetItalic */
        fprintf(out, "SetItalic hr %08lX\n", (unsigned long)hr);
        hr = ((set_long_function)font->vtable[53])(font, -1L);   /* SetStrikeThrough */
        fprintf(out, "SetStrikeThrough hr %08lX\n", (unsigned long)hr);
        hr = ((set_long_function)font->vtable[19])(font, (long)RGB(0, 255, 0));   /* SetBackColor */
        fprintf(out, "SetBackColor hr %08lX\n", (unsigned long)hr);
        selection.cpMin = 22;
        selection.cpMax = 23;
        SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
        memset(&format, 0, sizeof(format));
        format.cbSize = sizeof(format);
        SendMessage(editor, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        fprintf(out, "char 22: effects %08lX color %06lX back %06lX (want bold italic strike, 0000FF, 00FF00)\n",
                (unsigned long)format.dwEffects, (unsigned long)format.crTextColor,
                (unsigned long)format.crBackColor);
        selection.cpMin = 35;
        selection.cpMax = 36;
        SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
        SendMessage(editor, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        fprintf(out, "char 35: effects %08lX (want none)\n", (unsigned long)format.dwEffects);
    }
    if (range != NULL && font != NULL) {
        struct object *duplicate = NULL;
        struct object *para = NULL;
        struct object *para_duplicate = NULL;
        CHARFORMAT2 format;
        PARAFORMAT2 paragraph;
        CHARRANGE selection;
        HRESULT hr;
        long count = -1;

        hr = ((get_object_function)font->vtable[7])(font, (void **)&duplicate);   /* ITextFont::GetDuplicate */
        fprintf(out, "font GetDuplicate hr %08lX %p\n", (unsigned long)hr, (void *)duplicate);
        hr = ((set_long_function)duplicate->vtable[21])(duplicate, 0L);   /* SetBold(tomFalse) */
        hr = ((set_long_function)duplicate->vtable[25])(duplicate, (long)RGB(0, 0, 255));
        fprintf(out, "duplicate SetForeColor hr %08lX\n", (unsigned long)hr);
        hr = ((set_range_function)range->vtable[28])(range, 40L, 50L);
        hr = ((set_object_function)range->vtable[19])(range, duplicate);   /* ITextRange::SetFont */
        fprintf(out, "range SetFont hr %08lX\n", (unsigned long)hr);
        selection.cpMin = 45;
        selection.cpMax = 46;
        SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
        memset(&format, 0, sizeof(format));
        format.cbSize = sizeof(format);
        SendMessage(editor, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        fprintf(out, "char 45: effects %08lX color %06lX back %06lX (want not bold, FF0000, green)\n",
                (unsigned long)format.dwEffects, (unsigned long)format.crTextColor, (unsigned long)format.crBackColor);

        hr = ((get_object_function)range->vtable[20])(range, (void **)&para);   /* GetPara */
        fprintf(out, "GetPara hr %08lX %p\n", (unsigned long)hr, (void *)para);
        hr = ((get_object_function)para->vtable[7])(para, (void **)&para_duplicate);
        fprintf(out, "para GetDuplicate hr %08lX\n", (unsigned long)hr);
        hr = ((set_spacing_function)para_duplicate->vtable[43])(para_duplicate, 1L, 0.0f);   /* SetLineSpacing(1.5) */
        fprintf(out, "SetLineSpacing hr %08lX\n", (unsigned long)hr);
        hr = ((set_float_function)para_duplicate->vtable[45])(para_duplicate, 12.0f);   /* SetSpaceAfter 12pt */
        fprintf(out, "SetSpaceAfter hr %08lX\n", (unsigned long)hr);
        hr = ((set_object_function)range->vtable[21])(range, para_duplicate);   /* SetPara */
        fprintf(out, "SetPara hr %08lX\n", (unsigned long)hr);
        memset(&paragraph, 0, sizeof(paragraph));
        paragraph.cbSize = sizeof(paragraph);
        SendMessage(editor, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph);
        fprintf(out, "para at 45: rule %d spacing %ld after %ld (want 1, -, 240)\n", paragraph.bLineSpacingRule,
                (long)paragraph.dyLineSpacing, (long)paragraph.dySpaceAfter);
        selection.cpMin = 0;
        selection.cpMax = 0;
        SendMessage(editor, EM_EXSETSEL, 0, (LPARAM)&selection);
        SendMessage(editor, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph);
        fprintf(out, "para at 0: rule %d after %ld (want 0, 0)\n", paragraph.bLineSpacingRule, (long)paragraph.dySpaceAfter);

        hr = ((get_long_function)document->vtable[18])(document, &count);   /* Freeze */
        fprintf(out, "Freeze hr %08lX count %ld\n", (unsigned long)hr, count);
        hr = ((get_long_function)document->vtable[19])(document, &count);   /* Unfreeze */
        fprintf(out, "Unfreeze hr %08lX count %ld\n", (unsigned long)hr, count);
        fprintf(out, "release duplicate %lu\n", (unsigned long)((release_function)duplicate->vtable[2])(duplicate));
    }
    fclose(out);
    return 0;
}
