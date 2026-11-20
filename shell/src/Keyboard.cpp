/*
 * Keyboard: see Keyboard.h.
 */
#include "Keyboard.h"

#include "Glyphs.h"
#include "Theme.h"

#include "../../vkbd/VirtualKeyboard.h"

#include <InterfaceDefs.h>
#include <OS.h>

#include <ctype.h>
#include <string.h>


static const char* kLetterRows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char* kSymbolRows[] = { "1234567890", "@#$%&-+()", "*\"':;!?/" };

static const float kGap = 8;
static const float kRowHeight = 72;


// Haiku key codes for the US layout, so apps that look at "key" (shortcuts)
// get one.
static int32
key_code(char c)
{
	static const struct { const char* keys; int32 first; } kRows[] = {
		{ "1234567890", 0x12 }, { "qwertyuiop", 0x27 }, { "asdfghjkl", 0x3c },
		{ "zxcvbnm", 0x4c }
	};
	c = tolower(c);
	for (const auto& row : kRows) {
		if (const char* at = strchr(row.keys, c))
			return row.first + (at - row.keys);
	}
	return c == ' ' ? 0x5e : 0;
}


KeyboardView::KeyboardView(BRect frame)
	:
	BView(frame, "keyboard", B_FOLLOW_ALL, B_WILL_DRAW),
	fShift(false),
	fSymbols(false),
	fPressed(-1)
{
	SetViewColor(Theme::kPanel);
	_Layout();
}


void
KeyboardView::Draw(BRect updateRect)
{
	BFont font(be_plain_font);
	font.SetSize(26);
	SetFont(&font);
	font_height height;
	font.GetHeight(&height);

	for (size_t i = 0; i < fKeys.size(); i++) {
		const Key& key = fKeys[i];
		bool modifier = key.special != kCharacter && key.special != kSpace;
		rgb_color fill = modifier ? Theme::kSurface : Theme::kSurfaceRaised;
		if ((int32)i == fPressed)
			fill = Theme::kAccent;
		SetHighColor(fill);
		FillRoundRect(key.frame, 8, 8);

		BRect icon(0, 0, 30, 30);
		icon.OffsetTo(key.frame.left + (key.frame.Width() - 30) / 2,
			key.frame.top + (key.frame.Height() - 30) / 2);
		switch (key.special) {
			case kShift:
				draw_glyph(this, kGlyphShift, icon, Theme::kText, fShift ? 1 : 0);
				continue;
			case kBackspace:
				draw_glyph(this, kGlyphBackspace, icon, Theme::kText);
				continue;
			case kReturn:
				draw_glyph(this, kGlyphReturn, icon, Theme::kText);
				continue;
			default:
				break;
		}

		char label[8];
		strlcpy(label, key.label, sizeof(label));
		if (key.special == kCharacter && fShift)
			label[0] = toupper(label[0]);
		font.SetSize(key.special == kCharacter ? 26 : 18);
		SetFont(&font);
		font.GetHeight(&height);
		SetHighColor(Theme::kText);
		SetLowColor(fill);
		DrawString(label, BPoint(
			key.frame.left + (key.frame.Width() - font.StringWidth(label)) / 2,
			key.frame.top + (key.frame.Height() + height.ascent
				- height.descent) / 2));
	}
}


void
KeyboardView::MouseDown(BPoint where)
{
	fPressed = _KeyAt(where);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Invalidate();
}


void
KeyboardView::MouseUp(BPoint where)
{
	int32 pressed = fPressed;
	fPressed = -1;
	Invalidate();
	if (pressed < 0 || _KeyAt(where) != pressed)
		return;

	const Key& key = fKeys[pressed];
	switch (key.special) {
		case kCharacter:
		{
			char bytes[2] = { key.label[0], '\0' };
			if (fShift)
				bytes[0] = toupper(bytes[0]);
			_Send(bytes, key_code(bytes[0]));
			if (fShift) {
				fShift = false;
				Invalidate();
			}
			break;
		}
		case kSpace:
			_Send(" ", 0x5e);
			break;
		case kBackspace:
		{
			char bytes[2] = { B_BACKSPACE, '\0' };
			_Send(bytes, 0x1e);
			break;
		}
		case kReturn:
		{
			char bytes[2] = { B_RETURN, '\0' };
			_Send(bytes, 0x47);
			break;
		}
		case kShift:
			fShift = !fShift;
			break;
		case kSymbols:
			fSymbols = !fSymbols;
			fShift = false;
			_Layout();
			break;
	}
}


void
KeyboardView::_Layout()
{
	fKeys.clear();
	const char** rows = fSymbols ? kSymbolRows : kLetterRows;
	static char characters[3][16][2];

	BRect bounds = Bounds();
	float unit = (bounds.Width() - kGap * 11) / 10;
	for (int32 row = 0; row < 4; row++) {
		std::vector<Key> keys;
		if (row < 3) {
			if (row == 2)
				keys.push_back({ "", fSymbols ? kCharacter : kShift, 1.5f, BRect() });
			for (int32 i = 0; rows[row][i] != '\0'; i++) {
				characters[row][i][0] = rows[row][i];
				characters[row][i][1] = '\0';
				keys.push_back({ characters[row][i], kCharacter, 1, BRect() });
			}
			if (row == 2)
				keys.push_back({ "", kBackspace, 1.5f, BRect() });
			// The symbols layout has no shift: give its first key a symbol.
			if (row == 2 && fSymbols)
				keys[0].label = "=";
		} else {
			keys.push_back({ fSymbols ? "ABC" : "?123", kSymbols, 1.5f, BRect() });
			keys.push_back({ ",", kCharacter, 1, BRect() });
			keys.push_back({ "", kSpace, 4.5f, BRect() });
			keys.push_back({ ".", kCharacter, 1, BRect() });
			keys.push_back({ "", kReturn, 1.5f, BRect() });
		}

		float total = 0;
		for (const Key& key : keys)
			total += key.width * unit + kGap;
		float x = floorf((bounds.Width() - total + kGap) / 2);
		float y = kGap + row * (kRowHeight + kGap);
		for (Key& key : keys) {
			float width = key.width * unit;
			key.frame = BRect(x, y, x + width, y + kRowHeight);
			x += width + kGap;
			fKeys.push_back(key);
		}
	}
	Invalidate();
}


void
KeyboardView::_Send(const char* bytes, int32 code)
{
	port_id port = find_port(VIRTUAL_KEYBOARD_PORT);
	if (port < 0)
		return;

	virtual_key key = {};
	key.key = code;
	key.modifiers = fShift ? B_SHIFT_KEY | B_LEFT_SHIFT_KEY : 0;
	strlcpy(key.bytes, bytes, sizeof(key.bytes));
	write_port_etc(port, kVirtualKeyCode, &key, sizeof(key), B_RELATIVE_TIMEOUT,
		100000);
}


int32
KeyboardView::_KeyAt(BPoint where) const
{
	for (size_t i = 0; i < fKeys.size(); i++) {
		if (fKeys[i].frame.InsetByCopy(-kGap / 2, -kGap / 2).Contains(where))
			return i;
	}
	return -1;
}
