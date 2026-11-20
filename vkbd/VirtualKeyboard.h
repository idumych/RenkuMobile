/*
 * Between the shell and its input_server add-on. The shell's on-screen
 * keyboard writes keys to the add-on's port, which turns each into a key press
 * and release; the add-on tells the shell when a text field gets or loses
 * focus, so that it can show or hide the keyboard.
 *
 * The touch filter tells the shell while a finger is held still towards a
 * long press, so that it can show a ring filling up under the finger.
 */
#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <SupportDefs.h>


#define SHELL_SIGNATURE "application/x-vnd.Renku-MobileShell"
#define VIRTUAL_KEYBOARD_PORT "renku virtual keyboard"

enum {
	kVirtualKeyCode = 'vkey',
	kMsgTextFocus = 'txtF',		// to the shell; "text": bool
	kMsgTouchHold = 'tchH'		// to the shell; "where": BPoint, "since" and
								// "until": bigtime_t; no "where" when it ends
};

// The shell's ring window: presses go through it, so the touch filter looks
// past it for the window under a finger.
#define TOUCH_RING_NAME "Touch ring"

struct virtual_key {
	int32	key;		// Haiku key code, 0 when the character has none
	uint32	modifiers;	// held for this key only
	char	bytes[8];	// UTF-8, NUL-terminated
	char	raw;		// the character without the modifiers: 'c' for
						// Control-C, whose byte is 3
};


#endif	// VIRTUAL_KEYBOARD_H
