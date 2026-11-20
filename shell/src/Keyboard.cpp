/*
 * Keyboard: see Keyboard.h.
 */
#include "Keyboard.h"

#include "EmojiData.h"
#include "Glyphs.h"
#include "Theme.h"

#include "../../vkbd/VirtualKeyboard.h"

#include <Application.h>
#include <String.h>
#include <InterfaceDefs.h>
#include <OS.h>
#include <Window.h>

#include <algorithm>
#include <ctype.h>
#include <math.h>
#include <string.h>


static const char* kLetterRows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char* kSymbolRows[] = { "1234567890", "@#$%&-+()", "*\"':;!?/" };

static const float kGap = 8;
static const float kRowHeight = 72;
static const float kExtrasRowHeight = Theme::kKeyboardExtrasHeight - kGap;

// What the globe key's menu offers; the space bar shows the name.
static const struct {
	const char*	name;
	const char*	description;
	bool		extras;		// the row of terminal keys
} kLayouts[] = {
	{ "English", "US letters", false },
	{ "Terminal", "US letters, Ctrl, Tab and arrows", true }
};

// The globe key's menu, as Plasma's keyboard has it: a row per layout above
// the key, then Settings below a separator.
static const float kMenuWidth = 320;
// small enough for the menu to fit above the globe key on the English layout
static const float kMenuRowHeight = 54;
static const float kMenuSettingsHeight = 50;
static const float kMenuPadding = 6;
static const float kMenuSeparator = 9;
static const int32 kLayoutCount = sizeof(kLayouts) / sizeof(kLayouts[0]);
// after the layouts, as in Maliit's menu
static const int32 kMenuEmojiRow = kLayoutCount;
static const int32 kMenuSettingsRow = kLayoutCount + 1;

// The emoji bar's key for recent emoji: a stopwatch, as in Maliit.
static const char* kRecentIcon = "\xe2\x8f\xb1\xef\xb8\x8f";
// A finger that moves this far across the emoji scrolls them instead.
static const float kDragSlop = 20;


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
	BView(frame, "keyboard", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS),
	fShift(false),
	fSymbols(false),
	fLayout(0),
	fControl(false),
	fAlt(false),
	fPressed(-1),
	fMenu(false),
	fMenuPressed(-1),
	fEmoji(false),
	fPressedEmoji(NULL),
	fScrollStart(0),
	fPanelPressed(false),
	fDragging(false)
{
	SetViewColor(Theme::kPanel);
	_Layout();
}


void
KeyboardView::Draw(BRect updateRect)
{
	if (fEmoji)
		fEmojiPanel.Draw(this, fPressedEmoji);

	BFont font(be_plain_font);
	font_height height;

	for (size_t i = 0; i < fKeys.size(); i++) {
		const Key& key = fKeys[i];
		bool modifier = key.special != kCharacter && key.special != kSpace;
		rgb_color fill = modifier ? Theme::kSurface : Theme::kSurfaceRaised;
		// the category in view, tinted as the menu's current row is
		if (key.special == kCategory && key.code == fEmojiPanel.Section())
			fill = mix_color(Theme::kSurface, Theme::kAccent, 90);
		if ((int32)i == fPressed || (_IsOn(key) && key.special != kShift))
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
			case kLayout:
				draw_glyph(this, kGlyphGlobe, icon, Theme::kText);
				continue;
			default:
				break;
		}

		char label[16];
		strlcpy(label, key.label, sizeof(label));
		if (key.special == kCharacter && fShift)
			label[0] = toupper(label[0]);
		// characters, arrows and emoji are drawn large, names small
		bool large = key.special == kCharacter || key.special == kCategory
			|| (key.special == kNamed && (uint8)label[0] >= 0x80);
		font.SetSize(large ? 26 : 18);
		SetFont(&font);
		font.GetHeight(&height);
		SetHighColor(key.special == kSpace ? Theme::kTextDim : Theme::kText);
		SetLowColor(fill);
		DrawString(label, BPoint(
			key.frame.left + (key.frame.Width() - font.StringWidth(label)) / 2,
			key.frame.top + (key.frame.Height() + height.ascent
				- height.descent) / 2));
	}

	if (fMenu)
		_DrawMenu();
}


void
KeyboardView::FrameResized(float width, float height)
{
	_Layout();
}


void
KeyboardView::MouseDown(BPoint where)
{
	if (fMenu) {
		fMenuPressed = _MenuRowAt(where);
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		Invalidate();
		return;
	}

	fPressed = _KeyAt(where);
	if (fEmoji && fPressed < 0 && fEmojiPanel.Frame().Contains(where)) {
		fPanelPressed = true;
		fDragging = false;
		fDragStart = where;
		fScrollStart = fEmojiPanel.Scroll();
		fPressedEmoji = fEmojiPanel.EmojiAt(where);
	}
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Invalidate();
}


void
KeyboardView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (!fPanelPressed)
		return;

	float dx = where.x - fDragStart.x;
	if (!fDragging && fabsf(dx) > kDragSlop) {
		fDragging = true;
		fPressedEmoji = NULL;
	}
	if (fDragging) {
		fEmojiPanel.SetScroll(fScrollStart - dx);
		Invalidate();
	}
}


void
KeyboardView::MouseUp(BPoint where)
{
	if (fMenu) {
		// A row acts when the finger lifts on it; a press outside the menu
		// only closes it.
		int32 row = _MenuRowAt(where);
		int32 pressed = fMenuPressed;
		fMenuPressed = -1;
		if (row >= 0 && row == pressed) {
			if (row < kLayoutCount)
				_SetLayout(row);
			else if (row == kMenuEmojiRow) {
				fEmoji = true;
				_Layout();
			} else
				be_app->PostMessage(kMsgShowSettings);
		}
		if (row < 0 || row == pressed)
			CloseMenu();
		Invalidate();
		return;
	}

	if (fPanelPressed) {
		// A tap types the emoji and keeps it among the recent ones; a drag
		// comes to rest on a column.
		fPanelPressed = false;
		if (fDragging)
			fEmojiPanel.Snap();
		else if (fPressedEmoji != NULL
			&& fEmojiPanel.EmojiAt(where) == fPressedEmoji) {
			BString emoji(fPressedEmoji);
			_Send(emoji.String(), 0, 0);
			fEmojiPanel.AddRecent(emoji.String());
		}
		fPressedEmoji = NULL;
		Invalidate();
		return;
	}

	int32 pressed = fPressed;
	fPressed = -1;
	Invalidate();
	if (pressed < 0 || _KeyAt(where) != pressed)
		return;

	const Key& key = fKeys[pressed];
	bool typed = true;
	switch (key.special) {
		case kCharacter:
		{
			char c = fShift ? toupper(key.label[0]) : key.label[0];
			char bytes[2] = { c, '\0' };
			// Control and a letter type its control character, as a real
			// keyboard does.
			if (fControl && isalpha(c))
				bytes[0] = tolower(c) & 0x1f;
			_Send(bytes, key_code(c), c);
			break;
		}
		case kSpace:
			_Send(" ", 0x5e, ' ');
			break;
		case kBackspace:
		{
			char bytes[2] = { B_BACKSPACE, '\0' };
			_Send(bytes, 0x1e, B_BACKSPACE);
			break;
		}
		case kReturn:
		{
			char bytes[2] = { B_RETURN, '\0' };
			_Send(bytes, 0x47, B_RETURN);
			break;
		}
		case kNamed:
		{
			char bytes[2] = { key.byte, '\0' };
			_Send(bytes, key.code, key.byte);
			break;
		}
		case kShift:
			fShift = !fShift;
			typed = false;
			break;
		case kControl:
			fControl = !fControl;
			typed = false;
			break;
		case kAlt:
			fAlt = !fAlt;
			typed = false;
			break;
		case kSymbols:
			fSymbols = !fSymbols;
			fShift = false;
			_Layout();
			typed = false;
			break;
		case kLayout:
			fMenu = true;
			typed = false;
			break;
		case kLetters:
			fEmoji = false;
			_Layout();
			typed = false;
			break;
		case kCategory:
			fEmojiPanel.JumpTo(key.code);
			typed = false;
			break;
	}

	// Shift, Control and Alt hold for one key.
	if (typed && (fShift || fControl || fAlt)) {
		fShift = fControl = fAlt = false;
		Invalidate();
	}
}


void
KeyboardView::CloseMenu()
{
	if (!fMenu)
		return;
	fMenu = false;
	fMenuPressed = -1;
	Invalidate();
}


void
KeyboardView::_SetLayout(int32 layout)
{
	bool extras = kLayouts[fLayout].extras;
	fLayout = layout;
	float grow = (kLayouts[fLayout].extras ? 1 : 0) - (extras ? 1 : 0);
	if (grow == 0) {
		_Layout();
		return;
	}
	// The keyboard grows upwards, above the navigation panel, and the apps
	// move up with it; FrameResized() lays the keys out again.
	grow *= Theme::kKeyboardExtrasHeight;
	Window()->MoveBy(0, -grow);
	Window()->ResizeBy(0, grow);
	be_app->PostMessage(kMsgKeyboardResized);
}


void
KeyboardView::_Layout()
{
	fKeys.clear();
	const char** rows = fSymbols ? kSymbolRows : kLetterRows;
	static char characters[3][16][2];
	bool extras = kLayouts[fLayout].extras;

	BRect bounds = Bounds();
	float unit = (bounds.Width() - kGap * 11) / 10;
	float y = kGap;
	if (fEmoji) {
		// The emoji take the letters' place, whatever the layout; the bar of
		// categories takes the bottom row's.
		y = bounds.bottom - kGap - kRowHeight;
		fEmojiPanel.SetFrame(BRect(kGap, kGap, bounds.right - kGap,
			y - kGap - 1));
	}
	for (int32 row = fEmoji ? 3 : extras ? -1 : 0; row < 4; row++) {
		std::vector<Key> keys;
		float height = kRowHeight;
		if (row < 0) {
			height = kExtrasRowHeight;
			keys = {
				{ "Esc", kNamed, 1, BRect(), B_ESCAPE, 0x01 },
				{ "Tab", kNamed, 1, BRect(), B_TAB, 0x26 },
				{ "Ctrl", kControl, 1, BRect(), 0, 0 },
				{ "Alt", kAlt, 1, BRect(), 0, 0 },
				{ "Home", kNamed, 1, BRect(), B_HOME, 0x20 },
				{ "End", kNamed, 1, BRect(), B_END, 0x35 },
				{ "\xe2\x86\x90", kNamed, 1, BRect(), B_LEFT_ARROW, 0x61 },
				{ "\xe2\x86\x91", kNamed, 1, BRect(), B_UP_ARROW, 0x57 },
				{ "\xe2\x86\x93", kNamed, 1, BRect(), B_DOWN_ARROW, 0x62 },
				{ "\xe2\x86\x92", kNamed, 1, BRect(), B_RIGHT_ARROW, 0x63 }
			};
		} else if (row < 3) {
			if (row == 2) {
				keys.push_back({ "", fSymbols ? kCharacter : kShift, 1.5f,
					BRect(), 0, 0 });
			}
			for (int32 i = 0; rows[row][i] != '\0'; i++) {
				characters[row][i][0] = rows[row][i];
				characters[row][i][1] = '\0';
				keys.push_back({ characters[row][i], kCharacter, 1, BRect(), 0,
					0 });
			}
			if (row == 2)
				keys.push_back({ "", kBackspace, 1.5f, BRect(), 0, 0 });
			// The symbols layout has no shift: give its first key a symbol.
			if (row == 2 && fSymbols)
				keys[0].label = "=";
		} else if (fEmoji) {
			keys.push_back({ "ABC", kLetters, 1.5f, BRect(), 0, 0 });
			keys.push_back({ kRecentIcon, kCategory, 0.8f, BRect(), 0,
				EmojiPanel::kRecent });
			for (int32 i = 0; i < kEmojiCategoryCount; i++) {
				keys.push_back({ kEmojiCategories[i].icon, kCategory, 0.8f,
					BRect(), 0, i });
			}
			keys.push_back({ "", kBackspace, 1.5f, BRect(), 0, 0 });
		} else {
			keys = {
				{ fSymbols ? "ABC" : "?123", kSymbols, 1.5f, BRect(), 0, 0 },
				{ "", kLayout, 1, BRect(), 0, 0 },
				{ ",", kCharacter, 1, BRect(), 0, 0 },
				{ kLayouts[fLayout].name, kSpace, 3.5f, BRect(), 0, 0 },
				{ ".", kCharacter, 1, BRect(), 0, 0 },
				{ "", kReturn, 1.5f, BRect(), 0, 0 }
			};
		}

		float total = 0;
		for (const Key& key : keys)
			total += key.width * unit + kGap;
		float x = floorf((bounds.Width() - total + kGap) / 2);
		for (Key& key : keys) {
			float width = key.width * unit;
			key.frame = BRect(x, y, x + width, y + height);
			x += width + kGap;
			fKeys.push_back(key);
		}
		y += height + kGap;
	}
	Invalidate();
}


void
KeyboardView::_Send(const char* bytes, int32 code, char raw)
{
	port_id port = find_port(VIRTUAL_KEYBOARD_PORT);
	if (port < 0)
		return;

	virtual_key key = {};
	key.key = code;
	if (fShift)
		key.modifiers |= B_SHIFT_KEY | B_LEFT_SHIFT_KEY;
	if (fControl)
		key.modifiers |= B_CONTROL_KEY | B_LEFT_CONTROL_KEY;
	if (fAlt)
		key.modifiers |= B_COMMAND_KEY | B_LEFT_COMMAND_KEY;
	strlcpy(key.bytes, bytes, sizeof(key.bytes));
	key.raw = raw;
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


/*!	Above the globe key, centred on it, and kept inside the keyboard. */
BRect
KeyboardView::_MenuFrame() const
{
	BRect globe;
	for (const Key& key : fKeys) {
		if (key.special == kLayout)
			globe = key.frame;
	}

	float height = kMenuPadding * 2 + kMenuSettingsRow * kMenuRowHeight
		+ kMenuSeparator + kMenuSettingsHeight;
	BRect bounds = Bounds();
	float left = std::clamp(globe.left + (globe.Width() - kMenuWidth) / 2,
		kGap, bounds.right - kGap - kMenuWidth);
	float top = std::max(kGap, globe.top - kGap - height);
	return BRect(left, top, left + kMenuWidth, top + height);
}


/*!	The layouts' rows, then Emoji, then Settings below a separator. */
BRect
KeyboardView::_MenuRowFrame(int32 row) const
{
	BRect menu = _MenuFrame().InsetByCopy(kMenuPadding, kMenuPadding);
	float top = menu.top + row * kMenuRowHeight;
	float height = kMenuRowHeight;
	if (row == kMenuSettingsRow) {
		top += kMenuSeparator;
		height = kMenuSettingsHeight;
	}
	return BRect(menu.left, top, menu.right, top + height - 1);
}


int32
KeyboardView::_MenuRowAt(BPoint where) const
{
	for (int32 row = 0; row <= kMenuSettingsRow; row++) {
		if (_MenuRowFrame(row).Contains(where))
			return row;
	}
	return -1;
}


void
KeyboardView::_DrawMenu()
{
	BRect menu = _MenuFrame();
	SetHighColor(Theme::kSurface);
	FillRoundRect(menu, 10, 10);
	SetHighColor(Theme::kDisabled);
	StrokeRoundRect(menu, 10, 10);

	// The layout in use is tinted with the accent and outlined, as in
	// Plasma's; the row under the finger is tinted more.
	rgb_color tint = mix_color(Theme::kSurface, Theme::kAccent, 70);
	rgb_color pressed = mix_color(Theme::kSurface, Theme::kAccent, 140);

	BFont font(be_plain_font);
	font_height height;
	for (int32 row = 0; row <= kMenuSettingsRow; row++) {
		BRect frame = _MenuRowFrame(row);
		bool current = row == fLayout;
		if (row == fMenuPressed || current) {
			SetHighColor(row == fMenuPressed ? pressed : tint);
			FillRoundRect(frame, 6, 6);
		}
		if (current) {
			SetHighColor(Theme::kAccent);
			StrokeRoundRect(frame, 6, 6);
		}
		SetLowColor(row == fMenuPressed ? pressed : current ? tint
			: Theme::kSurface);

		if (row == kMenuSettingsRow) {
			SetHighColor(Theme::kDisabled);
			float line = frame.top - kMenuSeparator / 2;
			StrokeLine(BPoint(frame.left + 8, line),
				BPoint(frame.right - 8, line));

			BRect icon(frame.left + 14, frame.top + (frame.Height() - 26) / 2,
				frame.left + 40, frame.top + (frame.Height() + 26) / 2);
			draw_glyph(this, kGlyphGear, icon, Theme::kText);
			font.SetSize(20);
			SetFont(&font);
			font.GetHeight(&height);
			SetHighColor(Theme::kText);
			DrawString("Settings", BPoint(icon.right + 14, frame.top
				+ (frame.Height() + height.ascent - height.descent) / 2));
			continue;
		}

		bool emoji = row == kMenuEmojiRow;
		font.SetSize(22);
		SetFont(&font);
		SetHighColor(Theme::kText);
		DrawString(emoji ? "Emoji" : kLayouts[row].name,
			BPoint(frame.left + 14, frame.top + 24));
		font.SetSize(16);
		SetFont(&font);
		SetHighColor(Theme::kTextDim);
		DrawString(emoji ? "Smileys, animals, food and more"
			: kLayouts[row].description, BPoint(frame.left + 14,
			frame.top + 45));
	}
}


bool
KeyboardView::_IsOn(const Key& key) const
{
	switch (key.special) {
		case kShift:
			return fShift;
		case kControl:
			return fControl;
		case kAlt:
			return fAlt;
		default:
			return false;
	}
}
