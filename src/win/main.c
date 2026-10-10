#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <richedit.h>

#include "app.h"
#include "commands.h"
#include "editor.h"
#include "files.h"
#include "preview.h"
#include "resource.h"
#include "style.h"
#include "trace.h"
#include "undo.h"

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

void app_document_changed(void)
{
    if (!app.modified) {
        app.modified = 1;
        app_update_title();
    }
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
    style_settings_changed();
    preview_apply_settings();
    if (app.previewing) {
        preview_show();
    }
}

static void toggle_preview(void)
{
    if (app.previewing) {
        preview_hide();
    } else {
        preview_show();
    }
    update_menu_checks();
}

/* Commands that change or search the text work on the editor. */
static int needs_editor(int id)
{
    switch (id) {
    case IDM_FILE_NEW:
    case IDM_FILE_OPEN:
    case IDM_EDIT_UNDO:
    case IDM_EDIT_REDO:
    case IDM_EDIT_CUT:
    case IDM_EDIT_PASTE:
    case IDM_EDIT_FIND:
    case IDM_EDIT_FIND_NEXT:
    case IDM_EDIT_REPLACE:
    case IDM_VIEW_FOCUS:
    case IDM_VIEW_TYPEWRITER:
        return 1;
    default:
        return 0;
    }
}

static void toggle(int *setting)
{
    *setting = !*setting;
    update_menu_checks();
    style_settings_changed();
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
        style_settings_changed();
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
               APP_NAME " 0.1.2\n\nA minimal Markdown editor for Windows 98.\n"
               "Markdown parsing by md4c (MIT license).",
               "About " APP_NAME, MB_OK | MB_ICONINFORMATION);
}

static void toggle_fullscreen(void);

static void on_command(int id)
{
    if (app.previewing && needs_editor(id)) {
        preview_hide();
        update_menu_checks();
    }
    switch (id) {
    case IDM_FILE_NEW:          files_new(); break;
    case IDM_FILE_OPEN:         files_open(); break;
    case IDM_FILE_SAVE:         files_save(); break;
    case IDM_FILE_SAVE_AS:      files_save_as(); break;
    case IDM_FILE_EXPORT:       export_document(); break;
    case IDM_FILE_EXIT:         PostMessage(app.window, WM_CLOSE, 0, 0); break;
    case IDM_EDIT_UNDO:         undo_undo(); break;
    case IDM_EDIT_REDO:         undo_redo(); break;
    case IDM_EDIT_CUT:          SendMessage(app.editor, WM_CUT, 0, 0); break;
    case IDM_EDIT_COPY:         SendMessage(app.previewing ? app.preview : app.editor, WM_COPY, 0, 0); break;
    case IDM_EDIT_PASTE:        SendMessage(app.editor, EM_PASTESPECIAL, CF_TEXT, 0); break;
    case IDM_EDIT_SELECT_ALL:   SendMessage(app.previewing ? app.preview : app.editor, EM_SETSEL, 0, -1); break;
    case IDM_EDIT_FIND:         find_open(0); break;
    case IDM_EDIT_FIND_NEXT:    find_next(); break;
    case IDM_EDIT_REPLACE:      find_open(1); break;
    case IDM_VIEW_PREVIEW:      toggle_preview(); break;
    case IDM_VIEW_ESCAPE:
        /* Esc leaves the preview first, then full screen. */
        if (app.previewing) {
            toggle_preview();
        } else if (app.fullscreen) {
            toggle_fullscreen();
        }
        break;
    case IDM_VIEW_FULLSCREEN:      toggle_fullscreen(); break;
    case IDM_VIEW_COUNTERS:     counters_show(); break;
    case IDM_VIEW_NIGHT:
        app.settings.night = !app.settings.night;
        apply_view_settings();
        break;
    case IDM_VIEW_FOCUS:        toggle(&app.settings.focus); break;
    case IDM_VIEW_FOCUS_SCOPE:  toggle(&app.settings.focus_paragraph); break;
    case IDM_VIEW_TYPEWRITER:   toggle(&app.settings.typewriter); break;
    case IDM_VIEW_FONT:            choose_font(); break;
    case IDM_VIEW_FONT_LARGER:     change_font_size(1); break;
    case IDM_VIEW_FONT_SMALLER:    change_font_size(-1); break;
    case IDM_VIEW_COLUMN_WIDER:    change_column_width(2); break;
    case IDM_VIEW_COLUMN_NARROWER: change_column_width(-2); break;
    case IDM_HELP_ABOUT:           show_about(); break;
    default: break;
    }
}

static void resize_children(int width, int height)
{
    static int last_width = -1;
    static int last_height = -1;

    if (width == last_width && height == last_height) {
        return;
    }
    last_width = width;
    last_height = height;
    MoveWindow(app.editor, 0, 0, width, height, TRUE);
    editor_layout();
    if (app.preview != NULL) {
        MoveWindow(app.preview, 0, 0, width, height, TRUE);
        editor_layout_control(app.preview);
    }
    style_settings_changed();
}

/* Placement and menu of the normal window while in full screen. */
static WINDOWPLACEMENT windowed_placement;
static HMENU windowed_menu;

/* Distraction-free mode: a borderless popup covering the screen, no menu.
 * The editor keeps its centered column (WM_SIZE lays it out again). */
static void toggle_fullscreen(void)
{
    if (!app.fullscreen) {
        windowed_placement.length = sizeof(windowed_placement);
        GetWindowPlacement(app.window, &windowed_placement);
        windowed_menu = GetMenu(app.window);
        app.fullscreen = 1;
        SetMenu(app.window, NULL);
        SetWindowLong(app.window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(app.window, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                     GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        app.fullscreen = 0;
        SetWindowLong(app.window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetMenu(app.window, windowed_menu);
        SetWindowPlacement(app.window, &windowed_placement);
        SetWindowPos(app.window, NULL, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
    CheckMenuItem(app.fullscreen ? windowed_menu : GetMenu(app.window), IDM_VIEW_FULLSCREEN,
                  app.fullscreen ? MF_CHECKED : MF_UNCHECKED);
}

static void remember_placement(void)
{
    WINDOWPLACEMENT placement;

    placement.length = sizeof(placement);
    if (app.fullscreen) {
        placement = windowed_placement;
        app.settings.window = placement.rcNormalPosition;
        app.settings.maximized = placement.showCmd == SW_SHOWMAXIMIZED;
    } else if (GetWindowPlacement(app.window, &placement)) {
        app.settings.window = placement.rcNormalPosition;
        app.settings.maximized = placement.showCmd == SW_SHOWMAXIMIZED;
    }
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == find_message() && message != 0) {
        find_handle(lparam);
        return 0;
    }
    switch (message) {
    case WM_SIZE:
        if (app.editor != NULL) {
            resize_children(LOWORD(lparam), HIWORD(lparam));
        }
        return 0;
    case WM_NOTIFY:
        if (((NMHDR *)lparam)->hwndFrom == app.editor && ((NMHDR *)lparam)->code == EN_SELCHANGE
            && !style_busy) {
            style_view_changed();
        }
        return 0;
    case WM_SETFOCUS:
        if (app.editor != NULL) {
            SetFocus(app.previewing ? app.preview : app.editor);
        }
        return 0;
    case WM_COMMAND:
        if ((HWND)lparam == app.editor && app.editor != NULL) {
            if (HIWORD(wparam) == EN_CHANGE) {
                editor_notify_change();
            } else if (HIWORD(wparam) == EN_VSCROLL && !style_busy) {
                style_view_changed();
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
    WNDCLASSEX window_class;
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
    trace_open(app.settings.trace);
    app_theme_update();

    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_MINUTA));
    /* The hand-drawn 16x16 image, not a shrunken 32x32, in the title bar. */
    window_class.hIconSm = (HICON)LoadImage(instance, MAKEINTRESOURCE(IDI_MINUTA), IMAGE_ICON, 16, 16, 0);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
    window_class.lpszClassName = WINDOW_CLASS;
    if (!RegisterClassEx(&window_class)) {
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
    app.preview = preview_create(app.window);
    app.accelerators = LoadAccelerators(instance, MAKEINTRESOURCE(IDR_ACCELERATORS));
    /* The first WM_SIZE came before the editor existed: size it now, so
     * the document is styled for the real view before the window shows. */
    {
        RECT client;

        GetClientRect(app.window, &client);
        resize_children(client.right, client.bottom);
    }
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

    /* Styling runs only when no input is waiting, so typing never lags. */
    for (;;) {
        if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                break;
            }
            if (find_dialog() != NULL && IsDialogMessage(find_dialog(), &message)) {
                continue;
            }
            if (!TranslateAccelerator(app.window, app.accelerators, &message)) {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
        } else if (!style_idle()) {
            WaitMessage();
        }
    }
    document_format_free(&app.format);
    return (int)message.wParam;
}
