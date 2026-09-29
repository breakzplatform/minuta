#include "FindWindow.h"

#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <TextControl.h>

#include "Constants.h"

static const uint32 kMsgNext = 'fNxt';
static const uint32 kMsgPrevious = 'fPrv';


FindWindow::FindWindow(BWindow* target, const BString& text, bool caseSensitive)
	:
	BWindow(BRect(0, 0, 360, 100), "Find", B_FLOATING_WINDOW_LOOK,
		B_FLOATING_SUBSET_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE
			| B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTarget(target)
{
	AddToSubset(target);
	fText = new BTextControl("text", "Find:", text, NULL);
	fCaseSensitive = new BCheckBox("case", "Case sensitive", NULL);
	fCaseSensitive->SetValue(caseSensitive ? B_CONTROL_ON : B_CONTROL_OFF);
	BButton* next = new BButton("next", "Next", new BMessage(kMsgNext));
	BButton* previous = new BButton("previous", "Previous", new BMessage(kMsgPrevious));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fText)
		.AddGroup(B_HORIZONTAL)
			.Add(fCaseSensitive)
			.AddGlue()
			.Add(previous)
			.Add(next)
		.End();
	SetDefaultButton(next);
	fText->MakeFocus(true);
	fText->TextView()->SelectAll();

	BRect frame = target->Frame();
	MoveTo(frame.right - Bounds().Width() - 24, frame.top + 48);
}


void
FindWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgNext:
			_Send(false);
			break;
		case kMsgPrevious:
			_Send(true);
			break;
		case MSG_FIND:
			Activate();
			fText->MakeFocus(true);
			fText->TextView()->SelectAll();
			break;
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
FindWindow::_Send(bool backwards)
{
	BMessage request(MSG_FIND_REQUEST);
	request.AddString("text", fText->Text());
	request.AddBool("case", fCaseSensitive->Value() == B_CONTROL_ON);
	request.AddBool("backwards", backwards);
	fTarget.SendMessage(&request);
}
