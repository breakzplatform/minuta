#ifndef MINUTA_SETTINGS_H
#define MINUTA_SETTINGS_H

#include <Rect.h>
#include <String.h>

// Persistent preferences, a flattened BMessage in ~/config/settings/Minuta.
struct Settings {
	static const int32 kMinColumn = 50;
	static const int32 kMaxColumn = 80;
	static const int32 kDefaultColumn = 64;

	BRect	frame;
	float	fontSize;
	int32	columnWidth;
	bool	night;
	bool	focus;
	bool	focusParagraph;
	bool	typewriter;
	BString	lastFile;

			Settings();

	void	Load();
	void	Save() const;
};

extern Settings gSettings;

#endif
