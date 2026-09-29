#include <stdlib.h>

#include <windows.h>
#include <richedit.h>

#include "tom.h"

typedef void (*tom_method)(void);

struct tom_object {
    tom_method *vtable;
};

struct tom_font {
    struct tom_object *object;
};

typedef HRESULT (__stdcall *query_function)(void *self, const GUID *iid, void **out);
typedef ULONG (__stdcall *release_function)(void *self);
typedef HRESULT (__stdcall *range_function)(void *self, long start, long end, void **range);
typedef HRESULT (__stdcall *set_range_function)(void *self, long start, long end);
typedef HRESULT (__stdcall *get_object_function)(void *self, void **object);
typedef HRESULT (__stdcall *set_object_function)(void *self, void *object);
typedef HRESULT (__stdcall *set_long_function)(void *self, long value);
typedef HRESULT (__stdcall *set_float_function)(void *self, float value);
typedef HRESULT (__stdcall *set_spacing_function)(void *self, long rule, float value);

/* IUnknown */
#define QUERY_INTERFACE 0
#define RELEASE 2
/* ITextDocument */
#define DOCUMENT_RANGE 24
/* ITextRange */
#define RANGE_GET_FONT 18
#define RANGE_SET_FONT 19
#define RANGE_GET_PARA 20
#define RANGE_SET_PARA 21
#define RANGE_SET_RANGE 28
/* ITextFont and ITextPara */
#define GET_DUPLICATE 7
#define FONT_SET_BACK_COLOR 19
#define FONT_SET_BOLD 21
#define FONT_SET_FORE_COLOR 25
#define FONT_SET_ITALIC 31
#define FONT_SET_STRIKE_THROUGH 53
#define PARA_SET_LINE_SPACING 43
#define PARA_SET_SPACE_AFTER 45

#define TOM_TRUE (-1L)
#define TOM_FALSE 0L
#define TOM_LINE_SPACE_1PT5 1L

static const GUID text_document_iid = {
    0x8CC497C0, 0xA1DF, 0x11CE, { 0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D }
};

static struct tom_object *document;
static struct tom_object *range;

static void release(struct tom_object *object)
{
    if (object != NULL) {
        ((release_function)object->vtable[RELEASE])(object);
    }
}

int tom_open(HWND editor)
{
    struct tom_object *ole = NULL;

    tom_close();
    if (!SendMessage(editor, EM_GETOLEINTERFACE, 0, (LPARAM)&ole) || ole == NULL) {
        return 0;
    }
    if (FAILED(((query_function)ole->vtable[QUERY_INTERFACE])(ole, &text_document_iid, (void **)&document))) {
        document = NULL;
    }
    release(ole);
    if (document == NULL
        || FAILED(((range_function)document->vtable[DOCUMENT_RANGE])(document, 0, 0, (void **)&range))) {
        tom_close();
        return 0;
    }
    return 1;
}

void tom_close(void)
{
    release(range);
    release(document);
    range = NULL;
    document = NULL;
}

static int select_range(long start, long end)
{
    return range != NULL
        && SUCCEEDED(((set_range_function)range->vtable[RANGE_SET_RANGE])(range, start, end));
}

static void set_long(struct tom_object *object, int method, long value)
{
    ((set_long_function)object->vtable[method])(object, value);
}

struct tom_font *tom_font_create(long start, COLORREF color, int bold, int italic, int strike,
                                 long back_color)
{
    struct tom_object *live = NULL;
    struct tom_object *duplicate = NULL;
    struct tom_font *font;

    if (!select_range(start, start + 1)
        || FAILED(((get_object_function)range->vtable[RANGE_GET_FONT])(range, (void **)&live))) {
        return NULL;
    }
    if (FAILED(((get_object_function)live->vtable[GET_DUPLICATE])(live, (void **)&duplicate))) {
        duplicate = NULL;
    }
    release(live);
    font = duplicate != NULL ? malloc(sizeof(*font)) : NULL;
    if (font == NULL) {
        release(duplicate);
        return NULL;
    }
    font->object = duplicate;
    set_long(duplicate, FONT_SET_FORE_COLOR, (long)color);
    set_long(duplicate, FONT_SET_BACK_COLOR, back_color);
    set_long(duplicate, FONT_SET_BOLD, bold ? TOM_TRUE : TOM_FALSE);
    set_long(duplicate, FONT_SET_ITALIC, italic ? TOM_TRUE : TOM_FALSE);
    set_long(duplicate, FONT_SET_STRIKE_THROUGH, strike ? TOM_TRUE : TOM_FALSE);
    return font;
}

void tom_font_free(struct tom_font *font)
{
    if (font != NULL) {
        release(font->object);
        free(font);
    }
}

void tom_apply_font(long start, long end, struct tom_font *font)
{
    if (font != NULL && select_range(start, end)) {
        ((set_object_function)range->vtable[RANGE_SET_FONT])(range, font->object);
    }
}

void tom_apply_paragraph(long start, float space_after)
{
    struct tom_object *live = NULL;
    struct tom_object *duplicate = NULL;

    if (!select_range(start, start)
        || FAILED(((get_object_function)range->vtable[RANGE_GET_PARA])(range, (void **)&live))) {
        return;
    }
    if (SUCCEEDED(((get_object_function)live->vtable[GET_DUPLICATE])(live, (void **)&duplicate))) {
        ((set_spacing_function)duplicate->vtable[PARA_SET_LINE_SPACING])(duplicate, TOM_LINE_SPACE_1PT5, 0.0f);
        ((set_float_function)duplicate->vtable[PARA_SET_SPACE_AFTER])(duplicate, space_after);
        ((set_object_function)range->vtable[RANGE_SET_PARA])(range, duplicate);
        release(duplicate);
    }
    release(live);
}
