/*
 * ActionDrawer: see ActionDrawer.h.
 */
#include "ActionDrawer.h"

#include "Theme.h"

#include <Application.h>
#include <Roster.h>
#include <Window.h>

#include <math.h>
#include <time.h>


static const float kTileTop = 196;
static const float kTileHeight = 132;
static const float kSwipeClose = 40;


ActionDrawerView::ActionDrawerView(BRect frame)
	:
	OverlayView(frame, "action drawer"),
	fKeyboardShown(false),
	fTracking(false),
	fPressed(-1)
{
}


void
ActionDrawerView::Open()
{
	fStatus.Refresh();
	static_cast<OverlayWindow*>(Window())->Open(this);
	StartAnimation(true);
}


void
ActionDrawerView::Close()
{
	StartAnimation(false);
}


void
ActionDrawerView::SetKeyboardShown(bool shown)
{
	fKeyboardShown = shown;
	if (!Window()->IsHidden())
		Render();
}


void
ActionDrawerView::MouseDown(BPoint where)
{
	fTracking = true;
	fDownPoint = where;
	fPressed = _TileAt(where);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Render();
}


void
ActionDrawerView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	if (fTracking && fDownPoint.y - where.y > kSwipeClose) {
		fTracking = false;
		fPressed = -1;
		Close();
	}
}


void
ActionDrawerView::MouseUp(BPoint where)
{
	int32 pressed = fPressed;
	bool tap = fTracking;
	fTracking = false;
	fPressed = -1;
	if (!tap)
		return;

	if (where.y > _PanelHeight()) {
		Close();
		return;
	}
	if (pressed < 0 || _TileAt(where) != pressed) {
		Render();
		return;
	}

	switch (pressed) {
		case kTileNetwork:
			be_roster->Launch("application/x-vnd.Haiku-Network");
			Close();
			break;
		case kTileKeyboard:
			be_app->PostMessage(kMsgToggleKeyboard);
			Render();
			break;
		case kTileScreenshot:
			Close();
			be_app->PostMessage(kMsgTakeScreenshot);
			break;
		case kTileSettings:
			Close();
			be_app->PostMessage(kMsgShowSettings);
			break;
	}
}


void
ActionDrawerView::RenderContent(BView* view)
{
	BRect bounds = view->Bounds();
	float height = _PanelHeight();
	float offset = roundf((Progress() - 1) * height);

	BRect panel(0, offset, bounds.right, offset + height);
	view->SetHighColor(Theme::kPanel);
	view->FillRoundRect(panel.OffsetByCopy(0, -Theme::kCorner), Theme::kCorner,
		Theme::kCorner);

	// Date and time
	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);
	char text[64];
	strftime(text, sizeof(text), "%H:%M", &local);
	BFont font(be_plain_font);
	font.SetSize(48);
	font.SetFace(B_LIGHT_FACE);
	view->SetFont(&font);
	view->SetHighColor(Theme::kText);
	view->DrawString(text, BPoint(32, offset + 104));
	strftime(text, sizeof(text), "%A, %e %B", &local);
	font.SetSize(19);
	font.SetFace(B_REGULAR_FACE);
	view->SetFont(&font);
	view->SetHighColor(Theme::kTextDim);
	view->DrawString(text, BPoint(34, offset + 146));

	// Tiles
	struct { Glyph glyph; const char* label; } const kTiles[] = {
		{ fStatus.network == kNetworkWireless ? kGlyphWifi
			: fStatus.network == kNetworkWired ? kGlyphWired : kGlyphWifiOff,
			"Network" },
		{ kGlyphKeyboard, "Keyboard" },
		{ kGlyphCamera, "Screenshot" },
		{ kGlyphGear, "Settings" }
	};
	for (int32 i = 0; i < kTileCount; i++) {
		BRect frame = _TileFrame(i).OffsetByCopy(0, offset);
		bool on = (i == kTileNetwork && fStatus.network != kNetworkOffline)
			|| (i == kTileKeyboard && fKeyboardShown);
		rgb_color fill = on ? Theme::kAccent : Theme::kSurface;
		if (i == fPressed)
			fill = tint_color(fill, B_DARKEN_1_TINT);
		view->SetHighColor(fill);
		view->FillRoundRect(frame, Theme::kCorner, Theme::kCorner);

		BRect icon(0, 0, 36, 36);
		icon.OffsetTo(frame.left + (frame.Width() - 36) / 2, frame.top + 24);
		draw_glyph(view, kTiles[i].glyph, icon, Theme::kText);

		font.SetSize(15);
		view->SetFont(&font);
		float width = font.StringWidth(kTiles[i].label);
		view->SetHighColor(Theme::kText);
		view->DrawString(kTiles[i].label,
			BPoint(frame.left + (frame.Width() - width) / 2, frame.top + 92));

		if (i == kTileNetwork) {
			BString detail = fStatus.network == kNetworkOffline ? "Offline"
				: fStatus.networkName.String();
			font.SetSize(13);
			font.TruncateString(&detail, B_TRUNCATE_END, frame.Width() - 12);
			view->SetFont(&font);
			width = font.StringWidth(detail.String());
			view->SetHighColor(on ? Theme::kText : Theme::kTextDim);
			view->DrawString(detail.String(),
				BPoint(frame.left + (frame.Width() - width) / 2, frame.top + 114));
		}
	}

	// shortcut: no notification list, Renku's notification_server has no API
	// to read them back; add one there first.
	font.SetSize(17);
	view->SetFont(&font);
	view->SetHighColor(Theme::kTextDim);
	const char* empty = "No notifications";
	view->DrawString(empty, BPoint((bounds.Width() - font.StringWidth(empty)) / 2,
		offset + kTileTop + kTileHeight + 70));

	view->SetHighColor(Theme::kTextDim);
	view->FillRoundRect(BRect(bounds.Width() / 2 - 32, offset + height - 22,
		bounds.Width() / 2 + 32, offset + height - 17), 3, 3);
}


BRect
ActionDrawerView::_TileFrame(int32 tile) const
{
	float gap = 16;
	float width = floorf((Bounds().Width() - 2 * 24 - 3 * gap) / kTileCount);
	return BRect(24 + tile * (width + gap), kTileTop,
		24 + tile * (width + gap) + width, kTileTop + kTileHeight);
}


int32
ActionDrawerView::_TileAt(BPoint where) const
{
	for (int32 i = 0; i < kTileCount; i++) {
		if (_TileFrame(i).Contains(where))
			return i;
	}
	return -1;
}


float
ActionDrawerView::_PanelHeight() const
{
	return kTileTop + kTileHeight + 140;
}
