#ifndef MINUTA_COUNTERS_WINDOW_H
#define MINUTA_COUNTERS_WINDOW_H

#include <String.h>
#include <Window.h>

class BMessageRunner;

// Transient overlay near the bottom of a document window showing word and
// character counts; closes itself after a few seconds.
class CountersWindow : public BWindow {
public:
	static	void				ShowFor(BWindow* parent, const BString& text);

private:
								CountersWindow(BWindow* parent, const BString& text);
	virtual						~CountersWindow();

			BMessageRunner*		fCloser;
};

#endif
