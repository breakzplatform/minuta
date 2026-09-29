#ifndef MINUTA_EDITOR_VIEW_H
#define MINUTA_EDITOR_VIEW_H

#include <Font.h>
#include <TextView.h>

#include <string>
#include <vector>

#include "Theme.h"

struct scan_state;

// The Markdown editor: a BTextView that dims Markdown syntax (the core span
// scanner), fades everything outside the current sentence or paragraph in
// focus mode, keeps the caret line centered in typewriter mode and centers a
// fixed-width text column.
//
// Styling is incremental: an edit restyles only the paragraphs it touched,
// and scrolling restyles the paragraphs that become visible. Paragraphs out
// of sight may keep stale styles until they are scrolled into view.
class EditorView : public BTextView {
public:
								EditorView();

			void				SetDocument(const char* text, size_t size);
			bool				IsModified() const { return fModified; }
			void				SetModified(bool modified);

			void				SetNight(bool night);
			void				SetFocusMode(bool enabled, bool paragraph);
			void				SetTypewriter(bool enabled);
			void				SetFontSize(float size);
			void				SetColumnWidth(int32 characters);
			float				ColumnPixelWidth() const;

	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				KeyDown(const char* bytes, int32 numBytes);
	virtual	void				MouseUp(BPoint where);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				ScrollTo(BPoint where);
	virtual	void				Cut(BClipboard* clipboard);
	virtual	void				Paste(BClipboard* clipboard);
	virtual	void				Undo(BClipboard* clipboard);

protected:
	virtual	void				DoLayout();
	virtual	void				InsertText(const char* text, int32 length,
									int32 offset, const text_run_array* runs);
	virtual	void				DeleteText(int32 fromOffset, int32 toOffset);

private:
			void				_UpdateFonts();
			void				_UpdateInsets();
			void				_ApplyColors();

			int32				_ParagraphStart(int32 offset) const;
			int32				_ParagraphEnd(int32 offset) const;
			std::string			_TextRange(int32 start, int32 end) const;

			void				_RebuildFences();
			bool				_RescanFences(int32 start, int32 end);
			void				_StateAt(int32 offset, scan_state* state) const;

			void				_MarkDirty(int32 start, int32 end);
			void				_PostFlush();
			void				_Flush();
			void				_StyleParagraphs(int32 from, int32 to);
			void				_StyleVisible();
			void				_UpdateFocus();
			void				_CenterCaret();
			void				_AfterUserAction();

			const Theme*		fTheme;
			BFont				fPlainFont;
			BFont				fBoldFont;
			BFont				fItalicFont;
			BFont				fBoldItalicFont;
			float				fFontSize;
			float				fCharWidth;
			int32				fColumnWidth;
			bool				fFocus;
			bool				fFocusParagraph;
			int32				fFocusStart;
			int32				fFocusEnd;
			bool				fTypewriter;
			bool				fModified;
			bool				fLoading;
			bool				fStyling;
			bool				fFlushPosted;
			int32				fDirtyStart;
			int32				fDirtyEnd;
			bool				fVisibleDirty;
			// Start offsets of the lines that could open or close a fenced
			// code block, the only lines that carry scanner state forward.
			std::vector<int32>	fFences;
};

#endif
