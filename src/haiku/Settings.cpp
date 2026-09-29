#include "Settings.h"

#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

Settings gSettings;

static const char* kFileName = "Minuta";


Settings::Settings()
	:
	frame(80, 60, 80 + 760, 60 + 620),
	fontSize(12),
	columnWidth(kDefaultColumn),
	night(false),
	focus(false),
	focusParagraph(false),
	typewriter(false)
{
}


static status_t
settings_path(BPath& path)
{
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status == B_OK)
		status = path.Append(kFileName);
	return status;
}


void
Settings::Load()
{
	BPath path;
	if (settings_path(path) != B_OK)
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage archive;
	if (file.InitCheck() != B_OK || archive.Unflatten(&file) != B_OK)
		return;

	BRect savedFrame;
	if (archive.FindRect("frame", &savedFrame) == B_OK && savedFrame.IsValid())
		frame = savedFrame;
	float size;
	if (archive.FindFloat("font_size", &size) == B_OK && size >= 8 && size <= 36)
		fontSize = size;
	int32 column;
	if (archive.FindInt32("column_width", &column) == B_OK && column >= kMinColumn
		&& column <= kMaxColumn) {
		columnWidth = column;
	}
	archive.FindBool("night", &night);
	archive.FindBool("focus", &focus);
	archive.FindBool("focus_paragraph", &focusParagraph);
	archive.FindBool("typewriter", &typewriter);
	archive.FindString("last_file", &lastFile);
}


void
Settings::Save() const
{
	BPath path;
	if (settings_path(path) != B_OK)
		return;
	BMessage archive;
	archive.AddRect("frame", frame);
	archive.AddFloat("font_size", fontSize);
	archive.AddInt32("column_width", columnWidth);
	archive.AddBool("night", night);
	archive.AddBool("focus", focus);
	archive.AddBool("focus_paragraph", focusParagraph);
	archive.AddBool("typewriter", typewriter);
	archive.AddString("last_file", lastFile);
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		archive.Flatten(&file);
}
