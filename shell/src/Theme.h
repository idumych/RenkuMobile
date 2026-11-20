/*
 * Renku Mobile shell: colours, sizes and messages shared by every part.
 *
 * Sizes are for the original PinePhone's 720x1440 panel. The look borrows from
 * Plasma Mobile's Breeze Dark: a dark status bar and navigation panel, rounded
 * quick-setting tiles with a blue accent, and a wallpaper behind the home grid.
 */
#ifndef THEME_H
#define THEME_H

#include "../../vkbd/VirtualKeyboard.h"

#include <GraphicsDefs.h>
#include <Rect.h>


namespace Theme {

// Panels
const float kStatusBarHeight = 44;
const float kNavBarHeight = 64;
// with the suggestion strip on top
const float kKeyboardHeight = 378;
const float kSuggestionHeight = 48;
// what the keyboard's row of extra keys adds to it
const float kKeyboardExtrasHeight = 64;

// Home grid
const int32 kGridColumns = 4;
const float kIconSize = 88;
const float kCellHeight = 168;

// Breeze Dark
const rgb_color kPanel = {22, 25, 27, 255};
const rgb_color kSurface = {42, 46, 50, 255};
const rgb_color kSurfaceRaised = {55, 60, 66, 255};
const rgb_color kAccent = {61, 174, 233, 255};
const rgb_color kText = {252, 252, 252, 255};
const rgb_color kTextDim = {161, 169, 177, 255};
const rgb_color kDisabled = {90, 96, 102, 255};
const rgb_color kScrim = {0, 0, 0, 150};

const float kCorner = 18;

}	// namespace Theme


// Messages between the shell's windows and the application.
enum {
	kMsgGoHome = 'goHm',
	kMsgShowTasks = 'tskS',
	kMsgCloseActive = 'clsA',
	kMsgToggleKeyboard = 'kbdT',
	kMsgShowDrawer = 'drwS',
	kMsgLaunch = 'lnch',			// "ref": entry_ref
	kMsgActivateWindow = 'actW',	// "token": int32 server token
	kMsgCloseWindow = 'clsW',		// "token": int32 server token
	kMsgTakeScreenshot = 'shot',
	kMsgShowSettings = 'sett',
	kMsgKeyboardResized = 'kbdR',
	kMsgPulse = 'puls',
	kMsgAnimate = 'anim'
};


#endif	// THEME_H
