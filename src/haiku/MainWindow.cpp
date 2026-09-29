#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Beep.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Constants.h"
#include "CountersWindow.h"
#include "EditorView.h"
#include "FindWindow.h"
#include "PreviewView.h"
#include "Settings.h"

extern "C" {
#include "document_utf8.h"
#include "md4c-html.h"
#include "md4c.h"
#include "stats_utf8.h"
}

static const int32 kColumnWidths[] = { 50, 64, 72, 80 };
static const float kMinTextWidth = 320;
static const float kMinTextHeight = 200;
static const float kFontSizeStep = 1;
static const float kMinFontSize = 8;
static const float kMaxFontSize = 36;
// Files are read whole; anything larger is not a text document.
static const off_t kMaxFileSize = 64 * 1024 * 1024;

static const char* kExportStyle =
	"body{max-width:40em;margin:2em auto;padding:0 1em;"
	"font:16px/1.6 \"Noto Sans\",sans-serif;color:#1e1e1e;background:#f7f7f2}"
	"pre,code{font-family:\"Noto Sans Mono\",monospace;background:#ebebe4}"
	"pre{padding:.6em .8em;overflow:auto}pre code{background:none}"
	"blockquote{margin-left:0;padding-left:1em;border-left:3px solid #ccc;color:#666}"
	"a{color:#1f5fa8}img{max-width:100%}"
	"table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:.2em .6em}";


static BMenuItem*
add_item(BMenu* menu, const char* label, uint32 what, char shortcut = 0,
	uint32 modifiers = 0)
{
	BMenuItem* item = new BMenuItem(label, new BMessage(what), shortcut, modifiers);
	menu->AddItem(item);
	return item;
}


static void
append_html(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
	static_cast<std::string*>(userdata)->append(text, size);
}


static std::string
escape_html(const char* text)
{
	std::string escaped;
	for (; *text != '\0'; text++) {
		switch (*text) {
			case '&': escaped += "&amp;"; break;
			case '<': escaped += "&lt;"; break;
			case '>': escaped += "&gt;"; break;
			case '"': escaped += "&quot;"; break;
			default: escaped += *text; break;
		}
	}
	return escaped;
}


MainWindow::MainWindow(BRect frame)
	:
	BWindow(frame, "Minuta", B_DOCUMENT_WINDOW, B_ASYNCHRONOUS_CONTROLS),
	fSavePanel(NULL),
	fFindCaseSensitive(false),
	fHasRef(false),
	fCloseAfterSave(false),
	fOpenExport(false),
	fOpenPending(false),
	fFullScreen(false),
	fWindowedLook(B_DOCUMENT_WINDOW_LOOK)
{
	document_format_init(&fFormat);
	fFormat.line_ending = LINE_ENDING_LF;

	fEditor = new EditorView();
	fPreview = new PreviewView();
	// The text views' minimum sizes grow with their insets (half the view
	// height in typewriter mode); left to the layout, that would grow the
	// window on every resize.
	fEditor->SetExplicitMinSize(BSize(kMinTextWidth, kMinTextHeight));
	fPreview->SetExplicitMinSize(BSize(kMinTextWidth, kMinTextHeight));
	fEditorScroll = new BScrollView("editor scroll", fEditor, 0, false, true, B_NO_BORDER);
	fPreviewScroll = new BScrollView("preview scroll", fPreview, 0, false, true,
		B_NO_BORDER);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar = _BuildMenus())
		.Add(fEditorScroll)
		.Add(fPreviewScroll);
	fPreviewScroll->Hide();
	SetSizeLimits(kMinTextWidth, 32768, kMinTextHeight, 32768);
	// '+' needs Shift on most layouts, and a menu shortcut only matches
	// without it; accept Alt+= and Alt+Shift+= as well.
	AddShortcut('=', B_COMMAND_KEY, new BMessage(MSG_FONT_BIGGER));
	AddShortcut('+', B_COMMAND_KEY | B_SHIFT_KEY, new BMessage(MSG_FONT_BIGGER));

	_ApplySettings();
	_UpdateTitle();
	fEditor->MakeFocus(true);
}


MainWindow::~MainWindow()
{
	delete fSavePanel;
	document_format_free(&fFormat);
}


BMenuBar*
MainWindow::_BuildMenus()
{
	BMenuBar* bar = new BMenuBar("menu");

	BMenu* file = new BMenu("File");
	add_item(file, "New", MSG_NEW, 'N');
	add_item(file, "Open" B_UTF8_ELLIPSIS, MSG_OPEN, 'O');
	file->AddSeparatorItem();
	add_item(file, "Save", MSG_SAVE, 'S');
	add_item(file, "Save as" B_UTF8_ELLIPSIS, MSG_SAVE_AS, 'S', B_SHIFT_KEY);
	file->AddSeparatorItem();
	add_item(file, "Export HTML" B_UTF8_ELLIPSIS, MSG_EXPORT, 'E');
	add_item(file, "Export HTML and open" B_UTF8_ELLIPSIS, MSG_EXPORT_OPEN, 'E', B_SHIFT_KEY);
	file->AddSeparatorItem();
	add_item(file, "Close", B_QUIT_REQUESTED, 'W');
	BMenuItem* quit = new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q');
	quit->SetTarget(be_app);
	file->AddItem(quit);
	bar->AddItem(file);

	BMenu* edit = new BMenu("Edit");
	BMenuItem* item = add_item(edit, "Undo", B_UNDO, 'Z');
	item->SetTarget(NULL, this);
	edit->AddSeparatorItem();
	add_item(edit, "Cut", B_CUT, 'X')->SetTarget(NULL, this);
	add_item(edit, "Copy", B_COPY, 'C')->SetTarget(NULL, this);
	add_item(edit, "Paste", B_PASTE, 'V')->SetTarget(NULL, this);
	add_item(edit, "Select all", B_SELECT_ALL, 'A')->SetTarget(NULL, this);
	edit->AddSeparatorItem();
	add_item(edit, "Find" B_UTF8_ELLIPSIS, MSG_FIND, 'F');
	add_item(edit, "Find next", MSG_FIND_NEXT, 'G');
	add_item(edit, "Find previous", MSG_FIND_PREVIOUS, 'G', B_SHIFT_KEY);
	edit->AddSeparatorItem();
	add_item(edit, "Word count", MSG_COUNTERS, 'W', B_SHIFT_KEY);
	bar->AddItem(edit);

	BMenu* view = new BMenu("View");
	fPreviewItem = add_item(view, "Preview", MSG_TOGGLE_PREVIEW, 'R');
	fFullScreenItem = add_item(view, "Full screen", MSG_TOGGLE_FULL_SCREEN, B_ENTER);
	view->AddSeparatorItem();
	fFocusItem = add_item(view, "Focus mode", MSG_TOGGLE_FOCUS, 'F', B_SHIFT_KEY);
	fFocusParagraphItem = add_item(view, "Focus on paragraph", MSG_TOGGLE_FOCUS_PARAGRAPH,
		'P', B_SHIFT_KEY);
	fTypewriterItem = add_item(view, "Typewriter scrolling", MSG_TOGGLE_TYPEWRITER, 'T',
		B_SHIFT_KEY);
	fNightItem = add_item(view, "Night theme", MSG_TOGGLE_NIGHT, 'N', B_SHIFT_KEY);
	view->AddSeparatorItem();
	add_item(view, "Bigger text", MSG_FONT_BIGGER, '+');
	add_item(view, "Smaller text", MSG_FONT_SMALLER, '-');
	BMenu* columns = new BMenu("Column width");
	for (int32 i = 0; i < 4; i++) {
		BString label;
		label << kColumnWidths[i] << " characters";
		BMessage* message = new BMessage(MSG_COLUMN_WIDTH);
		message->AddInt32("width", kColumnWidths[i]);
		fColumnItems[i] = new BMenuItem(label, message);
		columns->AddItem(fColumnItems[i]);
	}
	columns->SetRadioMode(true);
	view->AddItem(columns);
	bar->AddItem(view);
	return bar;
}


void
MainWindow::_ApplySettings()
{
	fEditor->SetFontSize(gSettings.fontSize);
	fEditor->SetColumnWidth(gSettings.columnWidth);
	fEditor->SetNight(gSettings.night);
	fEditor->SetFocusMode(gSettings.focus, gSettings.focusParagraph);
	fEditor->SetTypewriter(gSettings.typewriter);
	fPreview->SetFontSize(gSettings.fontSize);
	fPreview->SetNight(gSettings.night);
	fPreview->SetColumnPixelWidth(fEditor->ColumnPixelWidth());
}


status_t
MainWindow::Open(const entry_ref& ref)
{
	BFile file(&ref, B_READ_ONLY);
	status_t status = file.InitCheck();
	off_t size = 0;
	if (status == B_OK)
		status = file.GetSize(&size);
	if (status == B_OK && size > kMaxFileSize)
		status = B_FILE_TOO_LARGE;
	if (status != B_OK) {
		_ShowError("open", status);
		return status;
	}
	std::string bytes((size_t)size, '\0');
	ssize_t read = size > 0 ? file.Read(&bytes[0], (size_t)size) : 0;
	if (read < 0 || read != size) {
		status = read < 0 ? (status_t)read : B_IO_ERROR;
		_ShowError("open", status);
		return status;
	}

	size_t textSize;
	char* text = document_decode_utf8(bytes.data(), bytes.size(), &fFormat, &textSize);
	if (text == NULL) {
		_ShowError("open", B_NO_MEMORY);
		return B_NO_MEMORY;
	}
	fEditor->SetDocument(text, textSize);
	free(text);
	fRef = ref;
	fHasRef = true;
	_UpdateTitle();
	if (fPreviewScroll->IsHidden() == false)
		_TogglePreview();

	BPath path(&ref);
	gSettings.lastFile = path.Path();
	return B_OK;
}


bool
MainWindow::IsBlank() const
{
	return !fOpenPending && !fHasRef && !fEditor->IsModified()
		&& fEditor->TextLength() == 0;
}


bool
MainWindow::HasRef(const entry_ref& ref) const
{
	return fHasRef && fRef == ref;
}


void
MainWindow::MenusBeginning()
{
	fPreviewItem->SetMarked(!fPreviewScroll->IsHidden());
	fFullScreenItem->SetMarked(fFullScreen);
	fNightItem->SetMarked(gSettings.night);
	fFocusItem->SetMarked(gSettings.focus);
	fFocusParagraphItem->SetMarked(gSettings.focusParagraph);
	fTypewriterItem->SetMarked(gSettings.typewriter);
	for (int32 i = 0; i < 4; i++)
		fColumnItems[i]->SetMarked(kColumnWidths[i] == gSettings.columnWidth);
	BWindow::MenusBeginning();
}


// F11 toggles full screen (Alt+Enter is the menu shortcut, as in
// Terminal), and Esc leaves it unless the preview is showing: there Esc
// returns to the editor first.
void
MainWindow::DispatchMessage(BMessage* message, BHandler* handler)
{
	if (message->what == B_KEY_DOWN || message->what == B_UNMAPPED_KEY_DOWN) {
		int32 key = message->GetInt32("key", 0);
		const char* bytes = message->GetString("bytes", "");
		if (key == B_F11_KEY) {
			_ToggleFullScreen();
			return;
		}
		if (fFullScreen && bytes[0] == B_ESCAPE && bytes[1] == '\0'
			&& fPreviewScroll->IsHidden()) {
			_ToggleFullScreen();
			return;
		}
	}
	BWindow::DispatchMessage(message, handler);
}


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_NEW:
		case MSG_OPEN:
			be_app->PostMessage(message);
			break;

		case MSG_OPEN_REF:
		{
			fOpenPending = false;
			entry_ref ref;
			if (message->FindRef("ref", &ref) == B_OK && Open(ref) != B_OK
				&& message->GetBool("close_on_failure", false)) {
				PostMessage(B_QUIT_REQUESTED);
			}
			break;
		}

		case MSG_SAVE:
			if (fHasRef)
				_Save();
			else
				_ShowSavePanel(B_SAVE_REQUESTED);
			break;
		case MSG_SAVE_AS:
			_ShowSavePanel(B_SAVE_REQUESTED);
			break;
		case B_SAVE_REQUESTED:
		{
			entry_ref directory;
			const char* name;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK
				&& _SaveTo(directory, name) == B_OK && fCloseAfterSave) {
				PostMessage(B_QUIT_REQUESTED);
			}
			fCloseAfterSave = false;
			break;
		}
		case MSG_EXPORT:
		case MSG_EXPORT_OPEN:
			fOpenExport = message->what == MSG_EXPORT_OPEN;
			_ShowSavePanel(MSG_EXPORT_REQUESTED);
			break;
		case MSG_EXPORT_REQUESTED:
		{
			entry_ref directory;
			const char* name;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				_Export(directory, name, fOpenExport);
			}
			break;
		}
		case B_CANCEL:
			fCloseAfterSave = false;
			break;

		case MSG_MODIFIED_CHANGED:
			_UpdateTitle();
			break;

		case MSG_TOGGLE_PREVIEW:
			_TogglePreview();
			break;
		case MSG_TOGGLE_FULL_SCREEN:
			_ToggleFullScreen();
			break;
		case MSG_TOGGLE_NIGHT:
			gSettings.night = !gSettings.night;
			fEditor->SetNight(gSettings.night);
			fPreview->SetNight(gSettings.night);
			break;
		case MSG_TOGGLE_FOCUS:
			gSettings.focus = !gSettings.focus;
			fEditor->SetFocusMode(gSettings.focus, gSettings.focusParagraph);
			break;
		case MSG_TOGGLE_FOCUS_PARAGRAPH:
			gSettings.focusParagraph = !gSettings.focusParagraph;
			fEditor->SetFocusMode(gSettings.focus, gSettings.focusParagraph);
			break;
		case MSG_TOGGLE_TYPEWRITER:
			gSettings.typewriter = !gSettings.typewriter;
			fEditor->SetTypewriter(gSettings.typewriter);
			break;
		case MSG_FONT_BIGGER:
		case MSG_FONT_SMALLER:
		{
			float size = gSettings.fontSize
				+ (message->what == MSG_FONT_BIGGER ? kFontSizeStep : -kFontSizeStep);
			if (size < kMinFontSize || size > kMaxFontSize)
				break;
			gSettings.fontSize = size;
			fEditor->SetFontSize(size);
			fPreview->SetFontSize(size);
			fPreview->SetColumnPixelWidth(fEditor->ColumnPixelWidth());
			break;
		}
		case MSG_COLUMN_WIDTH:
		{
			int32 width;
			if (message->FindInt32("width", &width) != B_OK)
				break;
			gSettings.columnWidth = width;
			fEditor->SetColumnWidth(width);
			fPreview->SetColumnPixelWidth(fEditor->ColumnPixelWidth());
			break;
		}

		case MSG_COUNTERS:
			_ShowCounters();
			break;

		case MSG_FIND:
		{
			if (fFindWindow.IsValid()) {
				fFindWindow.SendMessage(MSG_FIND);
				break;
			}
			FindWindow* window = new FindWindow(this, fFindText, fFindCaseSensitive);
			fFindWindow = BMessenger(window);
			window->Show();
			break;
		}
		case MSG_FIND_REQUEST:
		{
			const char* text;
			if (message->FindString("text", &text) != B_OK)
				break;
			fFindText = text;
			fFindCaseSensitive = message->GetBool("case", false);
			_Find(fFindText, fFindCaseSensitive, message->GetBool("backwards", false));
			break;
		}
		case MSG_FIND_NEXT:
		case MSG_FIND_PREVIOUS:
			if (fFindText.IsEmpty())
				PostMessage(MSG_FIND);
			else
				_Find(fFindText, fFindCaseSensitive, message->what == MSG_FIND_PREVIOUS);
			break;

		case B_REFS_RECEIVED:
		case B_SIMPLE_DATA:
			if (message->HasRef("refs")) {
				BMessage refs(*message);
				refs.what = B_REFS_RECEIVED;
				be_app->PostMessage(&refs);
				break;
			}
			BWindow::MessageReceived(message);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
MainWindow::QuitRequested()
{
	if (fEditor->IsModified()) {
		BString text;
		text << "Save changes to \"" << (fHasRef ? fRef.name : "Untitled") << "\"?";
		BAlert* alert = new BAlert("Minuta", text, "Cancel", "Don't save", "Save",
			B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_WARNING_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		alert->SetShortcut(1, 'd');
		int32 choice = alert->Go();
		if (choice == 0)
			return false;
		if (choice == 2) {
			if (!fHasRef) {
				fCloseAfterSave = true;
				_ShowSavePanel(B_SAVE_REQUESTED);
				return false;
			}
			if (_Save() != B_OK)
				return false;
		}
	}
	gSettings.frame = fFullScreen ? fWindowedFrame : Frame();
	if (fFindWindow.IsValid())
		fFindWindow.SendMessage(B_QUIT_REQUESTED);
	be_app->PostMessage(MSG_WINDOW_CLOSED);
	return true;
}


void
MainWindow::_UpdateTitle()
{
	BString title;
	if (fEditor->IsModified())
		title << "\xE2\x80\xA2 ";
	title << (fHasRef ? fRef.name : "Untitled") << " \xE2\x80\x94 Minuta";
	SetTitle(title);
}


void
MainWindow::_ShowSavePanel(uint32 what)
{
	if (fSavePanel == NULL) {
		BMessenger target(this);
		fSavePanel = new BFilePanel(B_SAVE_PANEL, &target, NULL, B_FILE_NODE, false);
	}
	BMessage message(what);
	fSavePanel->SetMessage(&message);

	BString name = fHasRef ? fRef.name : "Untitled.md";
	if (what == MSG_EXPORT_REQUESTED) {
		int32 dot = name.FindLast('.');
		if (dot > 0)
			name.Truncate(dot);
		name << ".html";
		fSavePanel->Window()->SetTitle("Minuta: Export HTML");
	} else
		fSavePanel->Window()->SetTitle("Minuta: Save");
	fSavePanel->SetSaveText(name);
	if (fHasRef) {
		BEntry entry(&fRef);
		BEntry parent;
		if (entry.GetParent(&parent) == B_OK)
			fSavePanel->SetPanelDirectory(&parent);
	}
	fSavePanel->Show();
}


status_t
MainWindow::_Save()
{
	BEntry entry(&fRef);
	BEntry parent;
	entry_ref directory;
	status_t status = entry.GetParent(&parent);
	if (status == B_OK)
		status = parent.GetRef(&directory);
	if (status != B_OK) {
		_ShowError("save", status);
		return status;
	}
	return _SaveTo(directory, fRef.name);
}


status_t
MainWindow::_SaveTo(const entry_ref& directoryRef, const char* name)
{
	size_t size;
	char* bytes = document_encode_utf8(fEditor->Text(), fEditor->TextLength(), &fFormat,
		&size);
	if (bytes == NULL) {
		_ShowError("save", B_NO_MEMORY);
		return B_NO_MEMORY;
	}

	BDirectory directory(&directoryRef);
	BFile file;
	status_t status = directory.InitCheck();
	if (status == B_OK)
		status = file.SetTo(&directory, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (status == B_OK) {
		ssize_t written = file.Write(bytes, size);
		if (written < 0)
			status = (status_t)written;
		else if ((size_t)written != size)
			status = B_IO_ERROR;
	}
	free(bytes);
	if (status == B_OK)
		_SetMarkdownType(file, name);
	if (status != B_OK) {
		_ShowError("save", status);
		return status;
	}

	BEntry entry(&directory, name);
	entry.GetRef(&fRef);
	fHasRef = true;
	fEditor->SetModified(false);
	_UpdateTitle();
	BPath path(&fRef);
	gSettings.lastFile = path.Path();
	return B_OK;
}


// Haiku's sniffer types most .md files as text/plain (content rules win
// over the extension), which would open them in the plain text editor.
// Files Minuta saves with a Markdown extension become text/markdown unless
// they already have another, more specific type.
void
MainWindow::_SetMarkdownType(BFile& file, const char* name)
{
	BString extension(name);
	int32 dot = extension.FindLast('.');
	extension.Remove(0, dot + 1);
	extension.ToLower();
	bool markdownName = dot > 0 && (extension == "md" || extension == "markdown");
	BNodeInfo info(&file);
	char type[B_MIME_TYPE_LENGTH];
	bool typed = info.GetType(type) == B_OK && type[0] != '\0';
	if (!typed || (markdownName && strcmp(type, "text/plain") == 0))
		info.SetType(MARKDOWN_TYPE);
}


void
MainWindow::_Export(const entry_ref& directoryRef, const char* name, bool open)
{
	std::string body;
	if (md_html(fEditor->Text(), (MD_SIZE)fEditor->TextLength(), append_html, &body,
			MD_DIALECT_GITHUB, 0) != 0) {
		_ShowError("export", B_ERROR);
		return;
	}
	std::string title = escape_html(fHasRef ? fRef.name : "Untitled");
	std::string html = "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n"
		"<title>" + title + "</title>\n<style>" + kExportStyle + "</style>\n</head>\n"
		"<body>\n" + body + "</body>\n</html>\n";

	BDirectory directory(&directoryRef);
	BFile file;
	status_t status = directory.InitCheck();
	if (status == B_OK)
		status = file.SetTo(&directory, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (status == B_OK) {
		ssize_t written = file.Write(html.data(), html.size());
		if (written < 0)
			status = (status_t)written;
		else if ((size_t)written != html.size())
			status = B_IO_ERROR;
	}
	if (status != B_OK) {
		_ShowError("export", status);
		return;
	}
	BNodeInfo(&file).SetType("text/html");
	if (open) {
		entry_ref ref;
		if (BEntry(&directory, name).GetRef(&ref) == B_OK)
			be_roster->Launch(&ref);
	}
}


void
MainWindow::_TogglePreview()
{
	if (fPreviewScroll->IsHidden()) {
		int32 start, end;
		fEditor->GetSelection(&start, &end);
		if (!fPreview->Render(fEditor->Text(), fEditor->TextLength())) {
			_ShowError("render the preview", B_ERROR);
			return;
		}
		fEditorScroll->Hide();
		fPreviewScroll->Show();
		int32 position = fPreview->SourceToText(end);
		fPreview->Select(position, position);
		fPreview->ScrollToSelection();
		fPreview->MakeFocus(true);
	} else {
		int32 start, end;
		fPreview->GetSelection(&start, &end);
		int32 source = fPreview->TextToSource(start);
		fPreviewScroll->Hide();
	SetSizeLimits(kMinTextWidth, 32768, kMinTextHeight, 32768);
		fEditorScroll->Show();
		fEditor->MakeFocus(true);
		if (source > 0) {
			fEditor->Select(source, source);
			fEditor->ScrollToSelection();
		}
	}
}


// Distraction-free mode: no border, no menu bar, the whole screen. The
// menu shortcuts keep working with the menu bar hidden, and the text
// column stays centered because the views recompute their insets on
// layout.
void
MainWindow::_ToggleFullScreen()
{
	if (!fFullScreen) {
		fWindowedFrame = Frame();
		fWindowedLook = Look();
		BRect screen = BScreen(this).Frame();
		fMenuBar->Hide();
		SetLook(B_NO_BORDER_WINDOW_LOOK);
		MoveTo(screen.LeftTop());
		ResizeTo(screen.Width(), screen.Height());
		fFullScreen = true;
	} else {
		SetLook(fWindowedLook);
		fMenuBar->Show();
		MoveTo(fWindowedFrame.LeftTop());
		ResizeTo(fWindowedFrame.Width(), fWindowedFrame.Height());
		fFullScreen = false;
	}
	Activate();
}


void
MainWindow::_ShowCounters()
{
	text_stats stats;
	text_stats_count_utf8(fEditor->Text(), fEditor->TextLength(), &stats);
	BString text;
	text << stats.words << (stats.words == 1 ? " word" : " words") << "  \xC2\xB7  "
		<< stats.characters << (stats.characters == 1 ? " character" : " characters")
		<< "  \xC2\xB7  " << stats.reading_minutes << " min read";
	CountersWindow::ShowFor(this, text);
}


void
MainWindow::_Find(const BString& needle, bool caseSensitive, bool backwards)
{
	BTextView* view = fPreviewScroll->IsHidden() ? (BTextView*)fEditor : (BTextView*)fPreview;
	if (needle.IsEmpty())
		return;
	BString haystack(view->Text(), view->TextLength());
	int32 start, end;
	view->GetSelection(&start, &end);
	int32 found;
	if (backwards) {
		found = caseSensitive ? haystack.FindLast(needle, start - 1)
			: haystack.IFindLast(needle, start - 1);
		if (found < 0) {
			found = caseSensitive ? haystack.FindLast(needle)
				: haystack.IFindLast(needle);
		}
	} else {
		found = caseSensitive ? haystack.FindFirst(needle, end)
			: haystack.IFindFirst(needle, end);
		if (found < 0) {
			found = caseSensitive ? haystack.FindFirst(needle)
				: haystack.IFindFirst(needle);
		}
	}
	if (found < 0) {
		beep();
		return;
	}
	view->Select(found, found + needle.Length());
	view->ScrollToSelection();
	view->MakeFocus(true);
	Activate();
}


void
MainWindow::_ShowError(const char* action, status_t status)
{
	BString text;
	text << "Could not " << action;
	if (fHasRef)
		text << " \"" << fRef.name << "\"";
	text << ": " << strerror(status);
	BAlert* alert = new BAlert("Minuta", text, "OK", NULL, NULL, B_WIDTH_AS_USUAL,
		B_STOP_ALERT);
	alert->Go(NULL);
}
