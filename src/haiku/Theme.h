#ifndef MINUTA_THEME_H
#define MINUTA_THEME_H

#include <GraphicsDefs.h>

struct Theme {
	rgb_color	background;
	rgb_color	text;
	rgb_color	marker;		// dimmed Markdown syntax
	rgb_color	faded;		// text outside the focus
	rgb_color	code;
	rgb_color	link;
	rgb_color	muted;		// preview quotes, rules, placeholders

	static const Theme& Get(bool night);
};

#endif
