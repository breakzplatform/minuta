#include <Application.h>
#include <Entry.h>
#include <FilePanel.h>
#include <MimeType.h>
#include <Path.h>
#include <Screen.h>
#include <Window.h>

#include <cstdio>
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
			if (--fWindowCount <= 0) {
				gSettings.Save();
				PostMessage(B_QUIT_REQUESTED);
			}
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

	MainWindow* window = blank != NULL ? blank : _NewWindow();
	if (!window->Lock())
		return;
	status_t status = window->Open(ref);
	if (window != blank) {
		if (status == B_OK) {
			window->Unlock();
			window->Show();
		} else {
			fWindowCount--;
			window->Quit();
		}
		return;
	}
	window->Unlock();
	window->Activate();
}


// Offers Minuta as the handler for Markdown files unless the user already
// picked one.
void
App::_RegisterFileType()
{
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
