#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#include "app.h"
#include "editor.h"
#include "files.h"
#include "resource.h"

#define WINDOW_CLASS "MinutaWindow"
#define AUTOSAVE_TIMER 1
#define AUTOSAVE_INTERVAL_MS 30000

struct app app;

void app_error(const char *format, const char *argument)
{
    char message[MAX_PATH + 128];

    wsprintf(message, format, argument);
    MessageBox(app.window, message, APP_NAME, MB_OK | MB_ICONEXCLAMATION);
}

void app_update_title(void)
{
    char title[MAX_PATH + 32];
    const char *name = "Untitled";

    if (app.path[0] != '\0') {
        const char *slash = strrchr(app.path, '\\');

        name = slash != NULL ? slash + 1 : app.path;
    }
    /* ASCII on purpose: the Windows 98 caption font (MS Sans Serif) has no
     * glyphs for CP1252's bullet and em dash and draws them as bars. */
    wsprintf(title, "%s%s - " APP_NAME, app.modified ? "* " : "", name);
    SetWindowText(app.window, title);
}

void app_theme_update(void)
{
    if (app.settings.night) {
        app.theme.text = RGB(0xD6, 0xD6, 0xD6);
        app.theme.background = RGB(0x1B, 0x1B, 0x1B);
        app.theme.marker = RGB(0x6C, 0x6C, 0x6C);
        app.theme.code_background = RGB(0x2A, 0x2A, 0x2A);
        app.theme.faded = RGB(0x55, 0x55, 0x55);
        app.theme.link = RGB(0x8A, 0xB4, 0xE0);
    } else {
        app.theme.text = RGB(0x1E, 0x1E, 0x1E);
        app.theme.background = RGB(0xF7, 0xF7, 0xF2);
        app.theme.marker = RGB(0xB0, 0xB0, 0xAA);
        app.theme.code_background = RGB(0xEA, 0xEA, 0xE3);
        app.theme.faded = RGB(0xC4, 0xC4, 0xBE);
        app.theme.link = RGB(0x1F, 0x5F, 0xAF);
    }
}

static void update_menu_checks(void)
{
    HMENU menu = GetMenu(app.window);

    CheckMenuItem(menu, IDM_VIEW_PREVIEW, app.previewing ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(menu, IDM_VIEW_FOCUS, app.settings.focus ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(menu, IDM_VIEW_FOCUS_SCOPE, app.settings.focus_paragraph ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(menu, IDM_VIEW_TYPEWRITER, app.settings.typewriter ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(menu, IDM_VIEW_NIGHT, app.settings.night ? MF_CHECKED : MF_UNCHECKED);
}

static void apply_view_settings(void)
{
    app_theme_update();
    editor_apply_settings();
    update_menu_checks();
}

static void change_font_size(int delta)
{
    int size = app.settings.font_size + delta;

    if (size >= FONT_SIZE_MIN && size <= FONT_SIZE_MAX) {
        app.settings.font_size = size;
        apply_view_settings();
    }
}

static void change_column_width(int delta)
{
    int width = app.settings.column_width + delta;

    if (width >= COLUMN_MIN && width <= COLUMN_MAX) {
        app.settings.column_width = width;
        editor_layout();
        InvalidateRect(app.editor, NULL, TRUE);
    }
}

static void choose_font(void)
{
    CHOOSEFONT dialog;
    LOGFONT font;
    HDC dc = GetDC(app.window);

    memset(&font, 0, sizeof(font));
    strcpy(font.lfFaceName, app.settings.font_name);
    font.lfHeight = -MulDiv(app.settings.font_size, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(app.window, dc);
    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = app.window;
    dialog.lpLogFont = &font;
    dialog.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE;
    dialog.nSizeMin = FONT_SIZE_MIN;
    dialog.nSizeMax = FONT_SIZE_MAX;
    if (ChooseFont(&dialog)) {
        strcpy(app.settings.font_name, font.lfFaceName);
        app.settings.font_size = dialog.iPointSize / 10;
        apply_view_settings();
    }
}

static void show_about(void)
{
    MessageBox(app.window,
               APP_NAME " 0.1.0\n\nA minimal Markdown editor for Windows 98.\n"
               "Markdown parsing by md4c (MIT license).",
               "About " APP_NAME, MB_OK | MB_ICONINFORMATION);
}

static void on_command(int id)
{
    switch (id) {
    case IDM_FILE_NEW:          files_new(); break;
    case IDM_FILE_OPEN:         files_open(); break;
    case IDM_FILE_SAVE:         files_save(); break;
    case IDM_FILE_SAVE_AS:      files_save_as(); break;
    case IDM_FILE_EXIT:         PostMessage(app.window, WM_CLOSE, 0, 0); break;
    case IDM_EDIT_UNDO:         SendMessage(app.editor, EM_UNDO, 0, 0); break;
    case IDM_EDIT_REDO:         SendMessage(app.editor, EM_REDO, 0, 0); break;
    case IDM_EDIT_CUT:          SendMessage(app.editor, WM_CUT, 0, 0); break;
    case IDM_EDIT_COPY:         SendMessage(app.editor, WM_COPY, 0, 0); break;
    case IDM_EDIT_PASTE:        SendMessage(app.editor, EM_PASTESPECIAL, CF_TEXT, 0); break;
    case IDM_EDIT_SELECT_ALL:   SendMessage(app.editor, EM_SETSEL, 0, -1); break;
    case IDM_VIEW_NIGHT:
        app.settings.night = !app.settings.night;
        apply_view_settings();
        break;
    case IDM_VIEW_FOCUS:
        app.settings.focus = !app.settings.focus;
        update_menu_checks();
        break;
    case IDM_VIEW_FOCUS_SCOPE:
        app.settings.focus_paragraph = !app.settings.focus_paragraph;
        update_menu_checks();
        break;
    case IDM_VIEW_TYPEWRITER:
        app.settings.typewriter = !app.settings.typewriter;
        update_menu_checks();
        break;
    case IDM_VIEW_FONT:            choose_font(); break;
    case IDM_VIEW_FONT_LARGER:     change_font_size(1); break;
    case IDM_VIEW_FONT_SMALLER:    change_font_size(-1); break;
    case IDM_VIEW_COLUMN_WIDER:    change_column_width(2); break;
    case IDM_VIEW_COLUMN_NARROWER: change_column_width(-2); break;
    case IDM_HELP_ABOUT:           show_about(); break;
    default: break;
    }
}

static void remember_placement(void)
{
    WINDOWPLACEMENT placement;

    placement.length = sizeof(placement);
    if (GetWindowPlacement(app.window, &placement)) {
        app.settings.window = placement.rcNormalPosition;
        app.settings.maximized = placement.showCmd == SW_SHOWMAXIMIZED;
    }
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_SIZE:
        if (app.editor != NULL) {
            MoveWindow(app.editor, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
            editor_layout();
        }
        return 0;
    case WM_SETFOCUS:
        if (app.editor != NULL) {
            SetFocus(app.editor);
        }
        return 0;
    case WM_COMMAND:
        if ((HWND)lparam == app.editor && app.editor != NULL) {
            if (HIWORD(wparam) == EN_CHANGE && !app.modified
                && SendMessage(app.editor, EM_GETMODIFY, 0, 0)) {
                app.modified = 1;
                app_update_title();
            }
            return 0;
        }
        on_command(LOWORD(wparam));
        return 0;
    case WM_TIMER:
        if (wparam == AUTOSAVE_TIMER) {
            files_autosave();
        }
        return 0;
    case WM_CLOSE:
        if (files_confirm_discard()) {
            remember_placement();
            settings_save(&app.settings);
            files_remove_autosave();
            DestroyWindow(window);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, AUTOSAVE_TIMER);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window, message, wparam, lparam);
    }
}

/* The first command line argument, without quotes. */
static void command_line_path(const char *command_line, char *path, size_t size)
{
    size_t length = 0;

    while (*command_line == ' ') {
        command_line++;
    }
    if (*command_line == '"') {
        command_line++;
        while (*command_line != '\0' && *command_line != '"' && length + 1 < size) {
            path[length++] = *command_line++;
        }
    } else {
        while (*command_line != '\0' && length + 1 < size) {
            path[length++] = *command_line++;
        }
        while (length > 0 && path[length - 1] == ' ') {
            length--;
        }
    }
    path[length] = '\0';
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    WNDCLASS window_class;
    MSG message;
    RECT *area;
    char path[MAX_PATH];
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = 640;
    int height = 520;

    (void)previous;
    app.instance = instance;
    document_format_init(&app.format);
    settings_load(&app.settings);
    app_theme_update();

    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_MINUTA));
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
    window_class.lpszClassName = WINDOW_CLASS;
    if (!RegisterClass(&window_class)) {
        return 1;
    }

    area = &app.settings.window;
    if (area->left != (LONG)CW_USEDEFAULT && area->top != (LONG)CW_USEDEFAULT) {
        x = area->left;
        y = area->top;
        width = area->right - area->left;
        height = area->bottom - area->top;
    } else {
        RECT work;

        /* First run: 90% of the work area, centered. */
        SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
        width = (work.right - work.left) * 9 / 10;
        height = (work.bottom - work.top) * 9 / 10;
        x = work.left + (work.right - work.left - width) / 2;
        y = work.top + (work.bottom - work.top - height) / 2;
    }
    app.window = CreateWindowEx(0, WINDOW_CLASS, APP_NAME, WS_OVERLAPPEDWINDOW, x, y, width, height,
                                NULL, NULL, instance, NULL);
    if (app.window == NULL) {
        return 1;
    }
    app.editor = editor_create(app.window);
    if (app.editor == NULL) {
        MessageBox(app.window, "Minuta needs RICHED20.DLL (RichEdit 2.0).", APP_NAME, MB_OK | MB_ICONSTOP);
        return 1;
    }
    app.accelerators = LoadAccelerators(instance, MAKEINTRESOURCE(IDR_ACCELERATORS));
    apply_view_settings();

    /* Load before showing, so the window paints once, with the text. */
    command_line_path(command_line, path, sizeof(path));
    if (path[0] == '\0' && GetFileAttributes(app.settings.last_file) != 0xFFFFFFFF) {
        strcpy(path, app.settings.last_file);
    }
    if (path[0] == '\0' || !files_open_path(path)) {
        app_update_title();
    }
    ShowWindow(app.window, app.settings.maximized ? SW_SHOWMAXIMIZED : show);
    UpdateWindow(app.window);
    SetTimer(app.window, AUTOSAVE_TIMER, AUTOSAVE_INTERVAL_MS, NULL);

    while (GetMessage(&message, NULL, 0, 0) > 0) {
        if (!TranslateAccelerator(app.window, app.accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
    document_format_free(&app.format);
    return (int)message.wParam;
}
