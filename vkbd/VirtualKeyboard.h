/*
 * The message the shell's on-screen keyboard writes to the input_server
 * add-on's port. The add-on turns each into a key press and release.
 */
#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <SupportDefs.h>


#define VIRTUAL_KEYBOARD_PORT "renku virtual keyboard"

enum { kVirtualKeyCode = 'vkey' };

struct virtual_key {
	int32	key;		// Haiku key code, 0 when the character has none
	uint32	modifiers;
	char	bytes[8];	// UTF-8, NUL-terminated
};


#endif	// VIRTUAL_KEYBOARD_H
