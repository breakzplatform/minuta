#include "EditorView.h"

#include <Application.h>
#include <Looper.h>
#include <Message.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Constants.h"

extern "C" {
#include "scanner.h"
#include "stats.h"
}

static const uint32 kMsgFlush = 'eFls';
static const float kMinSideInset = 16;
static const float kVerticalInset = 24;
// The longest prefix of a line that is read to update the fence state:
// fence lines are short, and only their start matters.
static const int32 kFenceLineLimit = 256;
// Shear for italics when the fixed font family has no italic style.
static const float kItalicShear = 105;


static bool
same_color(const rgb_color& a, const rgb_color& b)
{
	return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}


EditorView::EditorView()
	:
	BTextView("editor", B_WILL_DRAW | B_PULSE_NEEDED | B_FRAME_EVENTS | B_NAVIGABLE),
	fTheme(&Theme::Get(false)),
	fFontSize(12),
	fCharWidth(7),
	fColumnWidth(64),
	fFocus(false),
	fFocusParagraph(false),
	fFocusStart(0),
	fFocusEnd(0),
	fTypewriter(false),
	fModified(false),
	fLoading(false),
	fStyling(false),
	fFlushPosted(false),
	fDirtyStart(-1),
	fDirtyEnd(-1),
	fVisibleDirty(false)
{
	SetStylable(true);
	SetWordWrap(true);
	SetDoesUndo(true);
	SetAutoindent(false);
	_UpdateFonts();
}


void
EditorView::SetDocument(const char* text, size_t size)
{
	fLoading = true;
	SetText(text, (int32)size);
	fLoading = false;
	_RebuildFences();
	fDirtyStart = fDirtyEnd = -1;
	fFocusStart = fFocusEnd = 0;
	Select(0, 0);
	ScrollTo(BPoint(0, 0));
	SetModified(false);
	fVisibleDirty = true;
	_Flush();
}


void
EditorView::SetModified(bool modified)
{
	if (fModified == modified)
		return;
	fModified = modified;
	if (Window() != NULL)
		Window()->PostMessage(MSG_MODIFIED_CHANGED);
}


void
EditorView::SetNight(bool night)
{
	fTheme = &Theme::Get(night);
	_ApplyColors();
	fVisibleDirty = true;
	_Flush();
	Invalidate();
}


void
EditorView::SetFocusMode(bool enabled, bool paragraph)
{
	fFocus = enabled;
	fFocusParagraph = paragraph;
	fFocusStart = fFocusEnd = 0;
	fVisibleDirty = true;
	_Flush();
}


void
EditorView::SetTypewriter(bool enabled)
{
	fTypewriter = enabled;
	_UpdateInsets();
	_Flush();
}


void
EditorView::SetFontSize(float size)
{
	fFontSize = size;
	_UpdateFonts();
	_ApplyColors();
	_UpdateInsets();
	fVisibleDirty = true;
	_Flush();
}


void
EditorView::SetColumnWidth(int32 characters)
{
	fColumnWidth = characters;
	_UpdateInsets();
}


float
EditorView::ColumnPixelWidth() const
{
	return ceilf(fColumnWidth * fCharWidth);
}


void
EditorView::AttachedToWindow()
{
	BTextView::AttachedToWindow();
	_ApplyColors();
	_UpdateInsets();
}


void
EditorView::DoLayout()
{
	// Insets depend on the view size; setting them here, during layout,
	// avoids laying the text out once more after the window is shown.
	BTextView::DoLayout();
	_UpdateInsets();
}


void
EditorView::FrameResized(float width, float height)
{
	BTextView::FrameResized(width, height);
	_UpdateInsets();
	fVisibleDirty = true;
	_PostFlush();
}


void
EditorView::KeyDown(const char* bytes, int32 numBytes)
{
	BTextView::KeyDown(bytes, numBytes);
	_AfterUserAction();
}


void
EditorView::MouseUp(BPoint where)
{
	BTextView::MouseUp(where);
	_AfterUserAction();
}


void
EditorView::MessageReceived(BMessage* message)
{
	// Files dropped from Tracker open as documents instead of being
	// inserted as text.
	if (message->WasDropped() && message->HasRef("refs")) {
		BMessage refs(*message);
		refs.what = B_REFS_RECEIVED;
		be_app->PostMessage(&refs);
		return;
	}
	if (message->what == kMsgFlush) {
		fFlushPosted = false;
		_Flush();
		return;
	}
	BTextView::MessageReceived(message);
	if (message->WasDropped())
		_AfterUserAction();
}


void
EditorView::ScrollTo(BPoint where)
{
	BTextView::ScrollTo(where);
	if (!fStyling && Window() != NULL) {
		fVisibleDirty = true;
		_PostFlush();
	}
}


void
EditorView::Cut(BClipboard* clipboard)
{
	BTextView::Cut(clipboard);
	_AfterUserAction();
}


void
EditorView::Paste(BClipboard* clipboard)
{
	BTextView::Paste(clipboard);
	_AfterUserAction();
}


void
EditorView::Undo(BClipboard* clipboard)
{
	BTextView::Undo(clipboard);
	_AfterUserAction();
}


void
EditorView::InsertText(const char* text, int32 length, int32 offset,
	const text_run_array* runs)
{
	BTextView::InsertText(text, length, offset, runs);
	if (fLoading)
		return;
	for (size_t i = 0; i < fFences.size(); i++) {
		if (fFences[i] >= offset)
			fFences[i] += length;
	}
	int32 start = _ParagraphStart(offset);
	int32 end = _ParagraphEnd(offset + length);
	if (_RescanFences(start, end))
		fVisibleDirty = true;
	_MarkDirty(start, end);
	SetModified(true);
}


void
EditorView::DeleteText(int32 fromOffset, int32 toOffset)
{
	BTextView::DeleteText(fromOffset, toOffset);
	if (fLoading)
		return;
	int32 removed = toOffset - fromOffset;
	std::vector<int32> kept;
	kept.reserve(fFences.size());
	for (size_t i = 0; i < fFences.size(); i++) {
		if (fFences[i] < fromOffset)
			kept.push_back(fFences[i]);
		else if (fFences[i] >= toOffset)
			kept.push_back(fFences[i] - removed);
	}
	bool changed = kept.size() != fFences.size();
	fFences.swap(kept);
	int32 start = _ParagraphStart(fromOffset);
	int32 end = _ParagraphEnd(fromOffset);
	if (_RescanFences(start, end) || changed)
		fVisibleDirty = true;
	_MarkDirty(start, end);
	SetModified(true);
}


void
EditorView::_UpdateFonts()
{
	fPlainFont = *be_fixed_font;
	fPlainFont.SetSize(fFontSize);
	fCharWidth = fPlainFont.StringWidth("m");

	fBoldFont = fPlainFont;
	fBoldFont.SetFace(B_BOLD_FACE);

	fItalicFont = fPlainFont;
	fItalicFont.SetFace(B_ITALIC_FACE);
	font_family family;
	font_style style;
	fItalicFont.GetFamilyAndStyle(&family, &style);
	bool hasItalic = strstr(style, "Italic") != NULL || strstr(style, "Oblique") != NULL;
	if (!hasItalic) {
		fItalicFont = fPlainFont;
		fItalicFont.SetShear(kItalicShear);
	}
	fBoldItalicFont = fBoldFont;
	if (hasItalic)
		fBoldItalicFont.SetFace(B_BOLD_FACE | B_ITALIC_FACE);
	else
		fBoldItalicFont.SetShear(kItalicShear);
}


void
EditorView::_UpdateInsets()
{
	BRect bounds = Bounds();
	float side = floorf((bounds.Width() - ColumnPixelWidth()) / 2);
	if (side < kMinSideInset)
		side = kMinSideInset;
	float vertical = kVerticalInset;
	if (fTypewriter)
		vertical = std::max(kVerticalInset, floorf(bounds.Height() / 2));
	float left, top, right, bottom;
	GetInsets(&left, &top, &right, &bottom);
	if (left != side || right != side || top != vertical || bottom != vertical)
		SetInsets(side, vertical, side, vertical);
}


void
EditorView::_ApplyColors()
{
	SetViewColor(fTheme->background);
	SetLowColor(fTheme->background);
	// The style for newly typed text, until the next restyle.
	fStyling = true;
	SetFontAndColor(&fPlainFont, B_FONT_ALL, &fTheme->text);
	fStyling = false;
	if (Parent() != NULL) {
		Parent()->SetViewColor(fTheme->background);
		Parent()->Invalidate();
	}
}


int32
EditorView::_ParagraphStart(int32 offset) const
{
	if (offset > TextLength())
		offset = TextLength();
	while (offset > 0 && ByteAt(offset - 1) != '\n')
		offset--;
	return offset;
}


int32
EditorView::_ParagraphEnd(int32 offset) const
{
	int32 length = TextLength();
	if (offset < 0)
		offset = 0;
	while (offset < length && ByteAt(offset) != '\n')
		offset++;
	return offset;
}


std::string
EditorView::_TextRange(int32 start, int32 end) const
{
	std::string text;
	if (end > start) {
		text.resize(end - start);
		GetText(start, end - start, &text[0]);
	}
	return text;
}


void
EditorView::_RebuildFences()
{
	fFences.clear();
	const char* text = Text();
	int32 length = TextLength();
	int32 start = 0;
	while (start < length) {
		const char* newline = (const char*)memchr(text + start, '\n', length - start);
		int32 end = newline != NULL ? (int32)(newline - text) : length;
		if (scan_is_fence_line(text + start, end - start))
			fFences.push_back(start);
		start = end + 1;
	}
}


// Re-examines the lines from start to end (paragraph bounds), replacing the
// fences recorded there. Returns true if the set of fences changed.
bool
EditorView::_RescanFences(int32 start, int32 end)
{
	std::vector<int32>::iterator first
		= std::lower_bound(fFences.begin(), fFences.end(), start);
	std::vector<int32>::iterator last = std::upper_bound(first, fFences.end(), end);
	std::vector<int32> old(first, last);

	std::vector<int32> found;
	std::string text = _TextRange(start, end);
	size_t lineStart = 0;
	while (lineStart <= text.size()) {
		size_t lineEnd = text.find('\n', lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = text.size();
		if (scan_is_fence_line(text.data() + lineStart, lineEnd - lineStart))
			found.push_back(start + (int32)lineStart);
		lineStart = lineEnd + 1;
	}

	fFences.erase(first, last);
	fFences.insert(std::lower_bound(fFences.begin(), fFences.end(), start), found.begin(),
		found.end());
	return old != found;
}


void
EditorView::_StateAt(int32 offset, scan_state* state) const
{
	scan_state_init(state);
	char line[kFenceLineLimit];
	int32 length = TextLength();
	for (size_t i = 0; i < fFences.size() && fFences[i] < offset; i++) {
		int32 start = fFences[i];
		int32 size = 0;
		while (size < kFenceLineLimit && start + size < length
			&& ByteAt(start + size) != '\n') {
			size++;
		}
		GetText(start, size, line);
		scan_state_advance(state, line, size);
	}
}


void
EditorView::_MarkDirty(int32 start, int32 end)
{
	if (fDirtyStart < 0) {
		fDirtyStart = start;
		fDirtyEnd = end;
	} else {
		fDirtyStart = std::min(fDirtyStart, start);
		fDirtyEnd = std::max(fDirtyEnd, end);
	}
	_PostFlush();
}


void
EditorView::_PostFlush()
{
	if (fFlushPosted || Looper() == NULL)
		return;
	fFlushPosted = true;
	Looper()->PostMessage(kMsgFlush, this);
}


void
EditorView::_Flush()
{
	if (fStyling || Window() == NULL)
		return;
	if (fDirtyStart >= 0) {
		int32 length = TextLength();
		int32 start = std::min(fDirtyStart, length);
		int32 end = std::min(fDirtyEnd, length);
		fDirtyStart = fDirtyEnd = -1;
		_StyleParagraphs(start, end);
	}
	_UpdateFocus();
	if (fVisibleDirty) {
		fVisibleDirty = false;
		_StyleVisible();
	}
}


void
EditorView::_StyleVisible()
{
	BRect bounds = Bounds();
	_StyleParagraphs(OffsetAt(bounds.LeftTop()), OffsetAt(bounds.RightBottom()));
}


void
EditorView::_StyleParagraphs(int32 from, int32 to)
{
	int32 start = _ParagraphStart(from);
	int32 end = _ParagraphEnd(std::max(from, to));
	if (end <= start)
		return;

	std::string text = _TextRange(start, end);
	std::vector<unsigned char> styles(text.size(), 0);
	scan_state state;
	_StateAt(start, &state);
	size_t lineStart = 0;
	while (lineStart <= text.size()) {
		size_t lineEnd = text.find('\n', lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = text.size();
		scan_line(text.data() + lineStart, lineEnd - lineStart, &state, &styles[lineStart]);
		lineStart = lineEnd + 1;
	}

	// One run per stretch of equal style and focus.
	struct Run {
		int32 offset;
		unsigned char styles;
		bool faded;
	};
	std::vector<Run> runs;
	for (size_t i = 0; i < text.size(); i++) {
		int32 position = start + (int32)i;
		bool faded = fFocus && (position < fFocusStart || position >= fFocusEnd);
		unsigned char style = text[i] == '\n' ? 0 : styles[i];
		if (runs.empty() || runs.back().styles != style || runs.back().faded != faded) {
			Run run = { (int32)i, style, faded };
			runs.push_back(run);
		}
	}

	text_run_array* array = AllocRunArray((int32)runs.size());
	if (array == NULL)
		return;
	for (size_t i = 0; i < runs.size(); i++) {
		text_run& run = array->runs[i];
		unsigned char style = runs[i].styles;
		bool bold = (style & (SPAN_HEADING | SPAN_STRONG)) != 0;
		bool italic = (style & SPAN_EMPHASIS) != 0;
		run.offset = runs[i].offset;
		run.font = bold ? (italic ? fBoldItalicFont : fBoldFont)
			: (italic ? fItalicFont : fPlainFont);
		if (style & SPAN_STRIKE)
			run.font.SetFace(run.font.Face() | B_STRIKEOUT_FACE);
		if (runs[i].faded)
			run.color = fTheme->faded;
		else if (style & SPAN_MARKER)
			run.color = fTheme->marker;
		else if (style & SPAN_CODE)
			run.color = fTheme->code;
		else
			run.color = fTheme->text;
	}

	// Setting runs redraws the range; skip it when nothing changed, which is
	// the common case when scrolling over already styled text.
	bool same = false;
	int32 currentSize;
	text_run_array* current = RunArray(start, end, &currentSize);
	if (current != NULL && current->count == array->count) {
		same = true;
		for (int32 i = 0; same && i < array->count; i++) {
			const text_run& a = current->runs[i];
			const text_run& b = array->runs[i];
			same = a.offset == b.offset && a.font == b.font && same_color(a.color, b.color);
		}
	}
	FreeRunArray(current);
	if (!same) {
		fStyling = true;
		SetRunArray(start, end, array);
		fStyling = false;
	}
	FreeRunArray(array);
}


void
EditorView::_UpdateFocus()
{
	if (!fFocus)
		return;
	int32 selectionStart, caret;
	GetSelection(&selectionStart, &caret);
	int32 paragraphStart = _ParagraphStart(caret);
	int32 paragraphEnd = _ParagraphEnd(caret);
	int32 focusStart = paragraphStart;
	int32 focusEnd = paragraphEnd;
	if (!fFocusParagraph) {
		std::string paragraph = _TextRange(paragraphStart, paragraphEnd);
		size_t sentenceStart, sentenceEnd;
		sentence_bounds(paragraph.data(), paragraph.size(), caret - paragraphStart,
			&sentenceStart, &sentenceEnd);
		focusStart = paragraphStart + (int32)sentenceStart;
		focusEnd = paragraphStart + (int32)sentenceEnd;
	}
	if (focusStart == fFocusStart && focusEnd == fFocusEnd)
		return;
	int32 oldStart = std::min(fFocusStart, TextLength());
	int32 oldEnd = std::min(fFocusEnd, TextLength());
	fFocusStart = focusStart;
	fFocusEnd = focusEnd;
	_StyleParagraphs(oldStart, oldEnd);
	_StyleParagraphs(focusStart, focusEnd);
}


void
EditorView::_CenterCaret()
{
	int32 selectionStart, caret;
	GetSelection(&selectionStart, &caret);
	float height;
	BPoint point = PointAt(caret, &height);
	BRect bounds = Bounds();
	float target = floorf(point.y + height / 2 - bounds.Height() / 2);
	if (target < 0)
		target = 0;
	if (fabsf(target - bounds.top) >= 1)
		ScrollTo(BPoint(bounds.left, target));
}


void
EditorView::_AfterUserAction()
{
	_Flush();
	if (fTypewriter) {
		_CenterCaret();
		_Flush();
	}
}
