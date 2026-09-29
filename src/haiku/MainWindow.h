#ifndef MINUTA_MAIN_WINDOW_H
#define MINUTA_MAIN_WINDOW_H

#include <Entry.h>
#include <Messenger.h>
#include <String.h>
#include <Window.h>

extern "C" {
#include "document.h"
}

class BFile;
class BFilePanel;
class BMenuBar;
class BMenuItem;
class BScrollView;
class EditorView;
class FindWindow;
class PreviewView;

// One document per window: the editor, the preview that replaces it, the
// menus and the file handling.
class MainWindow : public BWindow {
public:
								MainWindow(BRect frame);
	virtual						~MainWindow();

			status_t			Open(const entry_ref& ref);
			bool				IsBlank() const;
			// Keeps IsBlank() false until an MSG_OPEN_REF sent to this
			// window has been handled.
			void				ReserveForOpen() { fOpenPending = true; }
			bool				HasRef(const entry_ref& ref) const;

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();
	virtual	void				MenusBeginning();
	virtual	void				DispatchMessage(BMessage* message, BHandler* handler);

private:
			BMenuBar*			_BuildMenus();
			void				_UpdateTitle();
			void				_ApplySettings();
			void				_ShowSavePanel(uint32 what);
			status_t			_Save();
			status_t			_SaveTo(const entry_ref& directory, const char* name);
			void				_SetMarkdownType(BFile& file, const char* name);
			void				_Export(const entry_ref& directory, const char* name,
									bool open);
			void				_TogglePreview();
			void				_ToggleFullScreen();
			void				_ShowCounters();
			void				_Find(const BString& text, bool caseSensitive,
									bool backwards);
			void				_ShowError(const char* action, status_t status);

			EditorView*			fEditor;
			PreviewView*		fPreview;
			BScrollView*		fEditorScroll;
			BScrollView*		fPreviewScroll;
			BMenuBar*			fMenuBar;
			BMenuItem*			fPreviewItem;
			BMenuItem*			fFullScreenItem;
			BMenuItem*			fNightItem;
			BMenuItem*			fFocusItem;
			BMenuItem*			fFocusParagraphItem;
			BMenuItem*			fTypewriterItem;
			BMenuItem*			fColumnItems[4];
			BFilePanel*			fSavePanel;
			BMessenger			fFindWindow;
			BString				fFindText;
			bool				fFindCaseSensitive;
			entry_ref			fRef;
			bool				fHasRef;
			document_format		fFormat;
			bool				fCloseAfterSave;
			bool				fOpenExport;
			bool				fOpenPending;
			bool				fFullScreen;
			BRect				fWindowedFrame;
			window_look			fWindowedLook;
};

#endif
