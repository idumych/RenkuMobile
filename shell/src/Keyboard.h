/*
 * Keyboard: the on-screen keyboard above the navigation panel. It never takes
 * focus, so what it types goes to the app that has it, through the
 * "Renku virtual keyboard" input_server add-on.
 *
 * As in Plasma's keyboard, the globe key beside the space bar opens a menu
 * of the layouts, with Settings below them, and the space bar names the
 * layout in use. The Terminal
 * layout adds a row above the letters with the keys a terminal wants:
 * Escape, Tab, Control, Alt (Haiku's Command key, as on a PC keyboard), Home,
 * End and the arrows. Control and Alt hold for the next key, as Shift does.
 * The window grows by the row, and the shell moves the apps up.
 *
 * The menu's Emoji, as in Maliit (Plasma Mobile's keyboard), puts emoji in
 * place of the letters (see EmojiPanel), with a bar of categories under
 * them, a key back to the letters and backspace. It stays until that key.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "EmojiPanel.h"

#include <View.h>

#include <vector>


class KeyboardView : public BView {
public:
								KeyboardView(BRect frame);

	virtual	void				Draw(BRect updateRect);
	virtual	void				FrameResized(float width, float height);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseUp(BPoint where);

			// When the keyboard goes, its menu goes with it.
			void				CloseMenu();

private:
	enum Special { kCharacter, kShift, kBackspace, kReturn, kSymbols,
		kSpace, kLayout, kControl, kAlt, kNamed, kLetters, kCategory };

	struct Key {
		const char*	label;
		Special		special;
		float		width;		// in key widths
		BRect		frame;
		char		byte;		// kNamed: what it types
		int32		code;		// kNamed: its key code; kCategory: its section
	};

			void				_Layout();
			void				_SetLayout(int32 layout);
			BRect				_MenuFrame() const;
			BRect				_MenuRowFrame(int32 row) const;
			int32				_MenuRowAt(BPoint where) const;
			void				_DrawMenu();
			void				_Send(const char* bytes, int32 key, char raw);
			int32				_KeyAt(BPoint where) const;
			bool				_IsOn(const Key& key) const;

			std::vector<Key>	fKeys;
			bool				fShift;
			bool				fSymbols;
			int32				fLayout;
			bool				fControl;
			bool				fAlt;
			int32				fPressed;
			bool				fMenu;
			int32				fMenuPressed;	// a row, or -1 outside

			bool				fEmoji;
			EmojiPanel			fEmojiPanel;
			const char*			fPressedEmoji;
			BPoint				fDragStart;
			float				fScrollStart;
			bool				fPanelPressed;	// tracking a finger on the grid
			bool				fDragging;
};


#endif	// KEYBOARD_H
