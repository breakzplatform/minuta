#ifndef MINUTA_CONSTANTS_H
#define MINUTA_CONSTANTS_H

#include <SupportDefs.h>

#define APP_SIGNATURE "application/x-vnd.Minuta"
#define MARKDOWN_TYPE "text/markdown"

enum {
	MSG_NEW = 'mNew',
	MSG_OPEN = 'mOpn',
	MSG_OPEN_REF = 'mORf',
	MSG_SAVE = 'mSav',
	MSG_SAVE_AS = 'mSvA',
	MSG_EXPORT = 'mExp',
	MSG_EXPORT_OPEN = 'mExO',
	MSG_EXPORT_REQUESTED = 'mExR',
	MSG_TOGGLE_PREVIEW = 'mPrv',
	MSG_TOGGLE_NIGHT = 'mNgt',
	MSG_TOGGLE_FOCUS = 'mFoc',
	MSG_TOGGLE_FOCUS_PARAGRAPH = 'mFoP',
	MSG_TOGGLE_TYPEWRITER = 'mTyp',
	MSG_TOGGLE_FULL_SCREEN = 'mFul',
	MSG_FONT_BIGGER = 'mFB+',
	MSG_FONT_SMALLER = 'mFB-',
	MSG_COLUMN_WIDTH = 'mCol',
	MSG_COUNTERS = 'mCnt',
	MSG_FIND = 'mFnd',
	MSG_FIND_NEXT = 'mFnN',
	MSG_FIND_PREVIOUS = 'mFnP',
	MSG_FIND_REQUEST = 'mFnR',
	MSG_MODIFIED_CHANGED = 'mMod',
	MSG_WINDOW_CLOSED = 'mWCl',
	MSG_CLOSE_OVERLAY = 'mOvC'
};

#endif
