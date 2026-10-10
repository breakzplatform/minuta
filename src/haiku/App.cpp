#include <AppFileInfo.h>
#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <Mime.h>
#include <Roster.h>
#include <MimeType.h>
#include <Path.h>
#include <Screen.h>
#include <Window.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "Constants.h"
#include "MainWindow.h"
#include "Settings.h"

static const float kCascadeStep = 24;


class App : public BApplication {
public:
								App();
	virtual						~App();

	virtual	void				ReadyToRun();
	virtual	void				ArgvReceived(int32 argc, char** argv);
	virtual	void				RefsReceived(BMessage* message);
	virtual	void				MessageReceived(BMessage* message);

private:
			MainWindow*			_NewWindow();
			void				_Open(const entry_ref& ref);
			void				_RegisterFileType();

			BFilePanel*			fOpenPanel;
			int32				fWindowCount;
};


App::App()
	:
	BApplication(APP_SIGNATURE),
	fOpenPanel(NULL),
	fWindowCount(0)
{
	gSettings.Load();
}


App::~App()
{
	delete fOpenPanel;
	gSettings.Save();
}


void
App::ReadyToRun()
{
	_RegisterFileType();
	if (fWindowCount > 0)
		return;
	entry_ref ref;
	if (gSettings.lastFile.Length() > 0
		&& get_ref_for_path(gSettings.lastFile, &ref) == B_OK && BEntry(&ref).Exists()) {
		_Open(ref);
	}
	if (fWindowCount == 0)
		_NewWindow()->Show();
}


void
App::ArgvReceived(int32 argc, char** argv)
{
	// A running instance gets later launches' arguments with their
	// working directory attached.
	BString directory;
	BMessage* message = CurrentMessage();
	if (message == NULL || message->FindString("cwd", &directory) != B_OK) {
		char buffer[B_PATH_NAME_LENGTH];
		if (getcwd(buffer, sizeof(buffer)) != NULL)
			directory = buffer;
	}
	for (int32 i = 1; i < argc; i++) {
		BPath path;
		if (argv[i][0] == '/')
			path.SetTo(argv[i]);
		else
			path.SetTo(directory, argv[i]);
		entry_ref ref;
		if (get_ref_for_path(path.Path(), &ref) == B_OK && BEntry(&ref).Exists())
			_Open(ref);
		else
			fprintf(stderr, "Minuta: %s: no such file\n", argv[i]);
	}
}


void
App::RefsReceived(BMessage* message)
{
	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
		_Open(ref);
}


void
App::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_NEW:
			_NewWindow()->Show();
			break;
		case MSG_OPEN:
			if (fOpenPanel == NULL) {
				fOpenPanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL, B_FILE_NODE, true);
				fOpenPanel->Window()->SetTitle("Minuta: Open");
			}
			fOpenPanel->Show();
			break;
		case MSG_WINDOW_CLOSED:
			if (--fWindowCount <= 0)
				PostMessage(B_QUIT_REQUESTED);
			break;
		default:
			BApplication::MessageReceived(message);
			break;
	}
}


MainWindow*
App::_NewWindow()
{
	BRect frame = gSettings.frame;
	frame.OffsetBy(kCascadeStep * (fWindowCount % 8), kCascadeStep * (fWindowCount % 8));
	BRect screen = BScreen().Frame();
	if (!screen.Contains(frame.LeftTop()))
		frame.OffsetTo(80, 60);
	fWindowCount++;
	return new MainWindow(frame);
}


// Activates the window that has the file, reuses an untouched blank window,
// or opens a new one.
void
App::_Open(const entry_ref& ref)
{
	MainWindow* blank = NULL;
	for (int32 i = 0; BWindow* window = WindowAt(i); i++) {
		MainWindow* document = dynamic_cast<MainWindow*>(window);
		if (document == NULL || !document->Lock())
			continue;
		bool hasRef = document->HasRef(ref);
		if (blank == NULL && document->IsBlank())
			blank = document;
		document->Unlock();
		if (hasRef) {
			document->Activate();
			return;
		}
	}

	// The window is shown before it loads the file, so the text is laid out
	// once, at the window's real size.
	MainWindow* window = blank;
	if (window == NULL) {
		window = _NewWindow();
		window->Show();
	}
	if (window->Lock()) {
		window->ReserveForOpen();
		window->Unlock();
	}
	BMessage open(MSG_OPEN_REF);
	open.AddRef("ref", &ref);
	open.AddBool("close_on_failure", window != blank);
	window->PostMessage(&open);
	window->Activate();
}


// True when the MIME database holds exactly the icons built into this
// executable: the app icon, and the icon for Markdown files.
static bool
RegisteredIconsMatch(const BMimeType& self, BAppFileInfo& executable)
{
	uint8* registered[2] = { NULL, NULL };
	size_t registeredSize[2] = { 0, 0 };
	uint8* built[2] = { NULL, NULL };
	size_t builtSize[2] = { 0, 0 };
	bool match = self.GetIcon(&registered[0], &registeredSize[0]) == B_OK
		&& self.GetIconForType(MARKDOWN_TYPE, &registered[1], &registeredSize[1]) == B_OK
		&& executable.GetIcon(&built[0], &builtSize[0]) == B_OK
		&& executable.GetIconForType(MARKDOWN_TYPE, &built[1], &builtSize[1]) == B_OK;
	for (int i = 0; i < 2; i++) {
		match = match && registeredSize[i] > 0 && registeredSize[i] == builtSize[i]
			&& memcmp(registered[i], built[i], builtSize[i]) == 0;
		free(registered[i]);
		free(built[i]);
	}
	return match;
}


// Offers Minuta as the handler for Markdown files unless the user already
// picked one. Activating a package doesn't enter its apps into the MIME
// database, an entry made by an older build keeps no icons, and one made by
// an older version keeps its icons after an upgrade, so Tracker would show
// generic or stale icons for the app and for .md files: unless our entry has
// this executable's icons, register the running executable (what
// `mimeset -a -F` does).
void
App::_RegisterFileType()
{
	BMimeType self(APP_SIGNATURE);
	app_info info;
	BFile file;
	BAppFileInfo executable;
	if (GetAppInfo(&info) == B_OK && file.SetTo(&info.ref, B_READ_ONLY) == B_OK
		&& executable.SetTo(&file) == B_OK && !RegisteredIconsMatch(self, executable)) {
		BPath path;
		if (path.SetTo(&info.ref) == B_OK)
			create_app_meta_mime(path.Path(), false, true, B_UPDATE_MIME_INFO_FORCE_UPDATE_ALL);
	}

	BMimeType type(MARKDOWN_TYPE);
	if (!type.IsInstalled())
		return;
	char preferred[B_MIME_TYPE_LENGTH];
	if (type.GetPreferredApp(preferred) != B_OK || preferred[0] == '\0')
		type.SetPreferredApp(APP_SIGNATURE);
}


int
main()
{
	App app;
	app.Run();
	return 0;
}
