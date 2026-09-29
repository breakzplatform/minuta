#include "PreviewView.h"

#include <Window.h>

#include <cmath>

#include "Constants.h"

extern "C" {
#include "md4c.h"
}

static const float kHeadingScale[] = { 1.6f, 1.35f, 1.15f, 1.05f, 1.0f, 1.0f };
static const float kMinSideInset = 16;
static const float kVerticalInset = 24;


PreviewView::PreviewView()
	:
	BTextView("preview", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
	fTheme(&Theme::Get(false)),
	fFontSize(12),
	fColumnWidth(480)
{
	SetStylable(true);
	SetWordWrap(true);
	MakeEditable(false);
	styled_text_init(&fStyled);
}


PreviewView::~PreviewView()
{
	styled_text_free(&fStyled);
}


bool
PreviewView::Render(const char* markdown, size_t size)
{
	if (styled_render(markdown, size, MD_DIALECT_GITHUB, &fStyled) != 0)
		return false;
	SetText(fStyled.text.data, (int32)fStyled.text.size);
	_ApplyStyles();
	return true;
}


void
PreviewView::SetNight(bool night)
{
	fTheme = &Theme::Get(night);
	SetViewColor(fTheme->background);
	SetLowColor(fTheme->background);
	if (Parent() != NULL)
		Parent()->SetViewColor(fTheme->background);
	_ApplyStyles();
	Invalidate();
}


void
PreviewView::SetFontSize(float size)
{
	fFontSize = size;
	_ApplyStyles();
}


void
PreviewView::SetColumnPixelWidth(float width)
{
	fColumnWidth = width;
	_UpdateInsets();
}


int32
PreviewView::SourceToText(int32 source) const
{
	return (int32)styled_source_to_text(&fStyled, (size_t)source);
}


int32
PreviewView::TextToSource(int32 position) const
{
	return (int32)styled_text_to_source(&fStyled, (size_t)position);
}


void
PreviewView::AttachedToWindow()
{
	BTextView::AttachedToWindow();
	SetViewColor(fTheme->background);
	SetLowColor(fTheme->background);
	_UpdateInsets();
}


void
PreviewView::FrameResized(float width, float height)
{
	BTextView::FrameResized(width, height);
	_UpdateInsets();
}


void
PreviewView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes == 1 && bytes[0] == B_ESCAPE) {
		Window()->PostMessage(MSG_TOGGLE_PREVIEW);
		return;
	}
	BTextView::KeyDown(bytes, numBytes);
}


void
PreviewView::_ApplyStyles()
{
	BFont plain(be_plain_font);
	plain.SetSize(fFontSize);
	BFont fixed(be_fixed_font);
	fixed.SetSize(fFontSize);

	// Unstyled stretches between the renderer's runs need runs too.
	int32 count = 1 + 2 * (int32)fStyled.run_count;
	text_run_array* array = AllocRunArray(count);
	if (array == NULL)
		return;
	int32 used = 0;
	size_t position = 0;
	for (size_t i = 0; i <= fStyled.run_count; i++) {
		size_t runStart = i < fStyled.run_count ? fStyled.runs[i].start : fStyled.text.size;
		if (runStart > position || used == 0) {
			text_run& gap = array->runs[used++];
			gap.offset = (int32)position;
			gap.font = plain;
			gap.color = fTheme->text;
		}
		if (i == fStyled.run_count)
			break;
		const styled_run& source = fStyled.runs[i];
		text_run& run = array->runs[used++];
		run.offset = (int32)source.start;
		run.font = (source.flags & (STYLED_CODE | STYLED_MONOSPACE)) ? fixed : plain;
		run.color = fTheme->text;
		uint16 face = 0;
		if (source.flags & STYLED_BOLD)
			face |= B_BOLD_FACE;
		if (source.flags & STYLED_ITALIC)
			face |= B_ITALIC_FACE;
		if (source.flags & (STYLED_LINK | STYLED_UNDERLINE))
			face |= B_UNDERSCORE_FACE;
		if (source.flags & STYLED_STRIKE)
			face |= B_STRIKEOUT_FACE;
		if (face != 0)
			run.font.SetFace(face);
		if (source.heading >= 1 && source.heading <= 6)
			run.font.SetSize(ceilf(fFontSize * kHeadingScale[source.heading - 1]));
		if (source.flags & STYLED_CODE)
			run.color = fTheme->code;
		else if (source.flags & STYLED_LINK)
			run.color = fTheme->link;
		else if (source.flags & STYLED_MUTED)
			run.color = fTheme->muted;
		position = source.start + source.length;
	}
	array->count = used;
	SetRunArray(0, TextLength(), array);
	FreeRunArray(array);
}


void
PreviewView::_UpdateInsets()
{
	float side = floorf((Bounds().Width() - fColumnWidth) / 2);
	if (side < kMinSideInset)
		side = kMinSideInset;
	SetInsets(side, kVerticalInset, side, kVerticalInset);
}
