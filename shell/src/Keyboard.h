/*
 * Keyboard: the on-screen keyboard above the navigation panel. It never takes
 * focus, so what it types goes to the app that has it, through the
 * "Renku virtual keyboard" input_server add-on.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <View.h>

#include <vector>


class KeyboardView : public BView {
public:
								KeyboardView(BRect frame);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);

private:
	enum Special { kCharacter, kShift, kBackspace, kReturn, kSymbols,
		kSpace };

	struct Key {
		const char*	label;
		Special		special;
		float		width;		// in key widths
		BRect		frame;
	};

			void				_Layout();
			void				_Send(const char* bytes, int32 key);
			int32				_KeyAt(BPoint where) const;

			std::vector<Key>	fKeys;
			bool				fShift;
			bool				fSymbols;
			int32				fPressed;
};


#endif	// KEYBOARD_H
