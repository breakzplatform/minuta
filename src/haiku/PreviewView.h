#ifndef MINUTA_PREVIEW_VIEW_H
#define MINUTA_PREVIEW_VIEW_H

#include <TextView.h>

extern "C" {
#include "styled.h"
}

#include "Theme.h"

// Read-only rendering of the document: md4c output from the core styled
// renderer, shown as styled text (no HTML engine).
class PreviewView : public BTextView {
public:
								PreviewView();
	virtual						~PreviewView();

			bool				Render(const char* markdown, size_t size);
			void				SetNight(bool night);
			void				SetFontSize(float size);
			void				SetColumnPixelWidth(float width);

			int32				SourceToText(int32 source) const;
			int32				TextToSource(int32 position) const;

	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				KeyDown(const char* bytes, int32 numBytes);

private:
			void				_ApplyStyles();
			void				_UpdateInsets();

			const Theme*		fTheme;
			float				fFontSize;
			float				fColumnWidth;
			styled_text			fStyled;
};

#endif
