#include "CountersWindow.h"

#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <StringView.h>

#include "Constants.h"

static const bigtime_t kVisibleTime = 4000000;


void
CountersWindow::ShowFor(BWindow* parent, const BString& text)
{
	(new CountersWindow(parent, text))->Show();
}


CountersWindow::CountersWindow(BWindow* parent, const BString& text)
	:
	BWindow(BRect(0, 0, 10, 10), "Counters", B_BORDERED_WINDOW_LOOK,
		B_FLOATING_SUBSET_WINDOW_FEEL, B_NOT_MOVABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE
			| B_NOT_MINIMIZABLE | B_NOT_RESIZABLE | B_AVOID_FOCUS
			| B_AUTO_UPDATE_SIZE_LIMITS),
	fCloser(NULL)
{
	AddToSubset(parent);
	BStringView* label = new BStringView("counters", text);
	BFont font(be_bold_font);
	label->SetFont(&font);
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
		.Add(label);
	Layout(true);
	ResizeToPreferred();

	BRect frame = parent->Frame();
	MoveTo(floorf(frame.left + (frame.Width() - Bounds().Width()) / 2),
		floorf(frame.bottom - Bounds().Height() - 40));
	BMessage close(B_QUIT_REQUESTED);
	fCloser = new BMessageRunner(BMessenger(this), &close, kVisibleTime, 1);
}


CountersWindow::~CountersWindow()
{
	delete fCloser;
}

