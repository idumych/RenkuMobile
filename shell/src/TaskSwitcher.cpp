/*
 * TaskSwitcher: see TaskSwitcher.h.
 */
#include "TaskSwitcher.h"

#include "Glyphs.h"
#include "Theme.h"

#include <Application.h>
#include <Bitmap.h>
#include <NodeInfo.h>
#include <Roster.h>
#include <Window.h>

#include <math.h>


static const float kCardHeight = 280;
static const float kGap = 24;
static const float kTop = Theme::kStatusBarHeight + 80;
static const float kIcon = 96;


TaskSwitcherView::TaskSwitcherView(BRect frame)
	:
	OverlayView(frame, "task switcher"),
	fPressed(-1),
	fPressedClose(false)
{
}


TaskSwitcherView::~TaskSwitcherView()
{
	_Clear();
}


void
TaskSwitcherView::Open(const std::vector<TrackedWindow>& windows)
{
	_Clear();

	float width = floorf((Bounds().Width() - 3 * kGap) / 2);
	int32 rows = (int32)((Bounds().Height() - Theme::kNavBarHeight - kTop)
		/ (kCardHeight + kGap));
	// shortcut: no scrolling, windows past the grid are left out; add a drag
	// to scroll when people keep that many open.
	for (const TrackedWindow& window : windows) {
		int32 index = fCards.size();
		if (index >= rows * 2)
			break;

		Card card;
		card.token = window.token;
		card.title = window.title;
		card.icon = NULL;
		app_info info;
		if (be_roster->GetRunningAppInfo(window.team, &info) == B_OK) {
			card.icon = new BBitmap(BRect(0, 0, kIcon - 1, kIcon - 1), B_RGBA32);
			if (BNodeInfo::GetTrackerIcon(&info.ref, card.icon,
					(icon_size)kIcon) != B_OK) {
				delete card.icon;
				card.icon = NULL;
			}
		}
		card.frame = BRect(0, 0, width, kCardHeight);
		card.frame.OffsetTo(kGap + (index % 2) * (width + kGap),
			kTop + (index / 2) * (kCardHeight + kGap));
		fCards.push_back(card);
	}

	static_cast<OverlayWindow*>(Window())->Open(this);
	StartAnimation(true);
}


void
TaskSwitcherView::Close()
{
	StartAnimation(false);
}


void
TaskSwitcherView::MouseDown(BPoint where)
{
	fPressed = -1;
	for (size_t i = 0; i < fCards.size(); i++) {
		if (fCards[i].frame.Contains(where)) {
			fPressed = i;
			fPressedClose = _CloseFrame(fCards[i]).Contains(where);
		}
	}
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	Render();
}


void
TaskSwitcherView::MouseUp(BPoint where)
{
	int32 pressed = fPressed;
	fPressed = -1;
	if (pressed < 0 || !fCards[pressed].frame.Contains(where)) {
		Close();
		return;
	}

	BMessage message(fPressedClose ? kMsgCloseWindow : kMsgActivateWindow);
	message.AddInt32("token", fCards[pressed].token);
	be_app->PostMessage(&message);
	if (fPressedClose) {
		delete fCards[pressed].icon;
		fCards.erase(fCards.begin() + pressed);
		if (fCards.empty())
			Close();
		else
			Render();
	} else
		Close();
}


void
TaskSwitcherView::RenderContent(BView* view)
{
	BRect bounds = view->Bounds();
	float offset = roundf((1 - Progress()) * 120);

	BFont font(be_bold_font);
	font.SetSize(26);
	view->SetFont(&font);
	view->SetHighColor(Theme::kText);
	view->DrawString("Open apps", BPoint(kGap + 4, kTop - 28 + offset));

	if (fCards.empty()) {
		font.SetSize(19);
		font.SetFace(B_REGULAR_FACE);
		view->SetFont(&font);
		const char* text = "Nothing is open";
		view->SetHighColor(Theme::kTextDim);
		view->DrawString(text, BPoint((bounds.Width() - font.StringWidth(text)) / 2,
			bounds.Height() / 2));
		return;
	}

	for (size_t i = 0; i < fCards.size(); i++) {
		const Card& card = fCards[i];
		BRect frame = card.frame.OffsetByCopy(0, offset);
		view->SetHighColor((int32)i == fPressed && !fPressedClose
			? Theme::kSurfaceRaised : Theme::kSurface);
		view->FillRoundRect(frame, Theme::kCorner, Theme::kCorner);

		BRect icon(0, 0, kIcon - 1, kIcon - 1);
		icon.OffsetTo(frame.left + floorf((frame.Width() - kIcon) / 2),
			frame.top + 66);
		if (card.icon != NULL)
			view->DrawBitmap(card.icon, icon);

		BFont title(be_plain_font);
		title.SetSize(17);
		BString text = card.title;
		title.TruncateString(&text, B_TRUNCATE_END, frame.Width() - 32);
		view->SetFont(&title);
		view->SetHighColor(Theme::kText);
		view->DrawString(text.String(), BPoint(frame.left
			+ (frame.Width() - title.StringWidth(text.String())) / 2,
			icon.bottom + 48));

		BRect close = _CloseFrame(card).OffsetByCopy(0, offset);
		if ((int32)i == fPressed && fPressedClose) {
			view->SetHighColor(Theme::kSurfaceRaised);
			view->FillEllipse(close);
		}
		draw_glyph(view, kGlyphClose, close.InsetByCopy(8, 8), Theme::kTextDim);
	}
}


BRect
TaskSwitcherView::_CloseFrame(const Card& card) const
{
	return BRect(card.frame.right - 56, card.frame.top + 8,
		card.frame.right - 8, card.frame.top + 56);
}


void
TaskSwitcherView::_Clear()
{
	for (Card& card : fCards)
		delete card.icon;
	fCards.clear();
}
