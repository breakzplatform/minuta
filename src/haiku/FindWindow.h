#ifndef MINUTA_FIND_WINDOW_H
#define MINUTA_FIND_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>

class BCheckBox;
class BTextControl;

// Small floating find panel; sends MSG_FIND_REQUEST to its document window.
class FindWindow : public BWindow {
public:
								FindWindow(BWindow* target, const BString& text,
									bool caseSensitive);

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Send(bool backwards);

			BMessenger			fTarget;
			BTextControl*		fText;
			BCheckBox*			fCaseSensitive;
};

#endif
