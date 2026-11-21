/*
 * TaskSwitcher: see TaskSwitcher.h.
 */
#include "TaskSwitcher.h"

#include "Glyphs.h"
#include "Theme.h"

#include <Application.h>
#include <Bitmap.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <Roster.h>
#include <Window.h>

#include <algorithm>
#include <math.h>


// tall enough for a phone-shaped preview beside the title
static const float kCardHeight = 420;
static const float kTitleHeight = 52;
static const uint32 kMsgRefreshPreviews = 'rfPv';
static const bigtime_t kRefreshInterval = 100000;
static const uint32 kMsgHideAfterPick = 'hdPk';
// how long the last frame of a pick's zoom stays while its app comes up
static const bigtime_t kPickHideDelay = 150000;

// Renku's compositor makes the previews with this private call; the system's
// libbe may not have it yet.
namespace BPrivate {
status_t get_window_preview(int32 windowToken, BBitmap* bitmap)
	__attribute__((weak));
}
static const float kGap = 24;
static const float kTop = Theme::kStatusBarHeight + 80;
static const float kIcon = 96;


TaskSwitcherView::TaskSwitcherView(BRect frame)
	:
	OverlayView(frame, "task switcher"),
	fRefresh(NULL),
	fZoomCard(-1),
	fZoom(NULL),
	fPicked(-1),
	fPressed(-1),
	fPressedClose(false)
{
}


TaskSwitcherView::~TaskSwitcherView()
{
	delete fRefresh;
	delete fZoom;
	_Clear();
	for (auto& preview : fPreviews)
		delete preview.second.bitmap;
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
		card.windowFrame = window.frame.OffsetByCopy(-Window()->Frame().left,
			-Window()->Frame().top);
		card.minimized = window.minimized;
		card.frame = BRect(0, 0, width, kCardHeight);
		card.frame.OffsetTo(kGap + (index % 2) * (width + kGap),
			kTop + (index / 2) * (kCardHeight + kGap));

		// The preview, as the window's shape, as large as the card allows
		// above the title; its bitmap is kept from the last time if it fits.
		BRect area = card.frame.InsetByCopy(8, 8);
		area.bottom -= kTitleHeight;
		float scale = std::min((area.Width() + 1) / (window.frame.Width() + 1),
			(area.Height() + 1) / (window.frame.Height() + 1));
		float previewWidth = floorf((window.frame.Width() + 1) * scale);
		float previewHeight = floorf((window.frame.Height() + 1) * scale);
		card.previewFrame = BRect(0, 0, previewWidth - 1, previewHeight - 1);
		card.previewFrame.OffsetTo(
			area.left + floorf((area.Width() + 1 - previewWidth) / 2),
			area.top + floorf((area.Height() + 1 - previewHeight) / 2));
		Preview& preview = fPreviews[window.token];
		if (preview.bitmap == NULL
			|| preview.bitmap->Bounds() != card.previewFrame.OffsetToCopy(0, 0)) {
			delete preview.bitmap;
			preview.bitmap = new BBitmap(card.previewFrame.OffsetToCopy(0, 0),
				B_RGB32);
			preview.shown = false;
		}
		fCards.push_back(card);
	}

	// The previews of windows that have gone go with them.
	for (auto it = fPreviews.begin(); it != fPreviews.end();) {
		bool open = false;
		for (const Card& card : fCards)
			open = open || card.token == it->first;
		if (open)
			++it;
		else {
			delete it->second.bitmap;
			it = fPreviews.erase(it);
		}
	}

	_RefreshPreviews();
	// The app open now shrinks into its card (the first: windows come front
	// to back), unless there's none, called from the home screen.
	delete fZoom;
	fZoom = NULL;
	fZoomCard = -1;
	if (!fCards.empty() && !fCards[0].minimized)
		_StartZoom(0);

	static_cast<OverlayWindow*>(Window())->Open(this);
	StartAnimation(true);
}


void
TaskSwitcherView::MessageReceived(BMessage* message)
{
	if (message->what == kMsgHideAfterPick) {
		delete fZoom;
		fZoom = NULL;
		fZoomCard = -1;
		Window()->Hide();
		return;
	}
	if (message->what != kMsgRefreshPreviews) {
		OverlayView::MessageReceived(message);
		return;
	}
	_RefreshPreviews();
	Render();
}


void
TaskSwitcherView::AnimationDone(bool opened)
{
	// The previews refresh while it's open, not while it animates: each
	// refresh draws it all again, and costs the animation its frames.
	if (opened && BPrivate::get_window_preview != NULL && fRefresh == NULL) {
		BMessage refresh(kMsgRefreshPreviews);
		fRefresh = new BMessageRunner(BMessenger(this), &refresh,
			kRefreshInterval);
	}
	if (!opened && fPicked >= 0) {
		// The zoom ends where the window is: it's brought up under the last
		// frame, which goes once it has.
		BMessage message(kMsgActivateWindow);
		message.AddInt32("token", fPicked);
		be_app->PostMessage(&message);
		fPicked = -1;
		BMessage hide(kMsgHideAfterPick);
		BMessageRunner::StartSending(BMessenger(this), &hide, kPickHideDelay,
			1);
		return;
	}

	// Zoomed in or out: the card goes back to its live preview.
	delete fZoom;
	fZoom = NULL;
	fZoomCard = -1;
	OverlayView::AnimationDone(opened);
}


/*!	Takes \a card's window at full size, to zoom from or to. False, and no
	zoom, where the compositor can't give it.
*/
bool
TaskSwitcherView::_StartZoom(int32 card)
{
	delete fZoom;
	fZoom = NULL;
	fZoomCard = -1;
	if (BPrivate::get_window_preview == NULL)
		return false;

	// at half the size: a quarter of the copying, and as it moves it doesn't
	// show
	BRect frame = fCards[card].windowFrame;
	BBitmap* zoom = new BBitmap(BRect(0, 0, floorf(frame.Width() / 2),
		floorf(frame.Height() / 2)), B_RGB32);
	if (zoom->InitCheck() != B_OK
		|| BPrivate::get_window_preview(fCards[card].token, zoom) != B_OK) {
		delete zoom;
		return false;
	}
	fZoom = zoom;
	fZoomCard = card;
	return true;
}


/*!	The windows' contents, as they are now, into the previews. A window that
	has none to give (its buffer is gone) keeps the one it had.
*/
void
TaskSwitcherView::_RefreshPreviews()
{
	if (BPrivate::get_window_preview == NULL)
		return;
	for (const Card& card : fCards) {
		Preview& preview = fPreviews[card.token];
		if (BPrivate::get_window_preview(card.token, preview.bitmap) == B_OK)
			preview.shown = true;
	}
}


void
TaskSwitcherView::Close()
{
	delete fRefresh;
	fRefresh = NULL;
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

	if (fPressedClose) {
		BMessage message(kMsgCloseWindow);
		message.AddInt32("token", fCards[pressed].token);
		be_app->PostMessage(&message);
		delete fCards[pressed].icon;
		fCards.erase(fCards.begin() + pressed);
		if (fCards.empty())
			Close();
		else
			Render();
	} else {
		// The card grows back to the screen; its app comes up when it's
		// there, not before: bringing it up (and the keyboard with it) keeps
		// app_server too busy for the zoom's frames.
		delete fRefresh;
		fRefresh = NULL;
		fPicked = fCards[pressed].token;
		_StartZoom(pressed);
		StartAnimation(false);
	}
}


void
TaskSwitcherView::RenderContent(BView* view)
{
	BRect bounds = view->Bounds();
	float progress = Progress();
	bool zooming = fZoomCard >= 0 && fZoomCard < (int32)fCards.size();
	// Without a zoom the cards slide up into place; with one, they fade.
	float offset = zooming ? 0 : roundf((1 - progress) * 120);
	uint8 fade = zooming ? (uint8)(255 * progress) : 255;
	auto faded = [fade](rgb_color color) {
		color.alpha = color.alpha * fade / 255;
		return color;
	};

	BFont font(be_bold_font);
	font.SetSize(26);
	view->SetFont(&font);
	view->SetHighColor(faded(Theme::kText));
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
		view->SetHighColor(faded((int32)i == fPressed && !fPressedClose
			? Theme::kSurfaceRaised : Theme::kSurface));
		view->FillRoundRect(frame, Theme::kCorner, Theme::kCorner);

		// the live preview, or the app's icon until there is one
		BRect previewFrame = card.previewFrame.OffsetByCopy(0, offset);
		auto preview = fPreviews.find(card.token);
		if ((int32)i == fZoomCard)
			;	// drawn zooming, over the rest
		else if (preview != fPreviews.end() && preview->second.shown) {
			_DrawFaded(view, preview->second.bitmap, previewFrame, fade);
		} else if (card.icon != NULL) {
			BRect icon(0, 0, kIcon - 1, kIcon - 1);
			icon.OffsetTo(previewFrame.left
					+ floorf((previewFrame.Width() - kIcon) / 2),
				previewFrame.top + floorf((previewFrame.Height() - kIcon) / 2));
			view->DrawBitmap(card.icon, icon);
		}

		BFont title(be_plain_font);
		title.SetSize(17);
		BString text = card.title;
		title.TruncateString(&text, B_TRUNCATE_END, frame.Width() - 32);
		view->SetFont(&title);
		view->SetHighColor(faded(Theme::kText));
		view->DrawString(text.String(), BPoint(frame.left
			+ (frame.Width() - title.StringWidth(text.String())) / 2,
			frame.bottom - 20));

		// over the preview, on a dark disc
		BRect close = _CloseFrame(card).OffsetByCopy(0, offset);
		view->SetHighColor(faded((int32)i == fPressed && fPressedClose
			? Theme::kSurfaceRaised : Theme::kSurface));
		view->FillEllipse(close);
		draw_glyph(view, kGlyphClose, close.InsetByCopy(8, 8),
			faded(Theme::kTextDim));
	}

	// The zooming window, between where it is on the screen (0) and its
	// card's preview (1).
	if (zooming) {
		const Card& card = fCards[fZoomCard];
		BRect from = card.windowFrame, to = card.previewFrame;
		BRect rect(from.left + (to.left - from.left) * progress,
			from.top + (to.top - from.top) * progress,
			from.right + (to.right - from.right) * progress,
			from.bottom + (to.bottom - from.bottom) * progress);
		// unfiltered: in motion it doesn't show, and filtering a screen's
		// worth each frame halves the frames
		view->DrawBitmap(fZoom, fZoom->Bounds(), rect);
	}
}


/*!	A preview, as see-through as the cards are while they fade in or out:
	its pixels are opaque, so the alpha is the high colour's.
*/
void
TaskSwitcherView::_DrawFaded(BView* view, BBitmap* bitmap, BRect frame,
	uint8 alpha)
{
	if (alpha == 255) {
		view->DrawBitmap(bitmap, frame);
		return;
	}
	source_alpha source;
	alpha_function function;
	view->GetBlendingMode(&source, &function);
	view->SetBlendingMode(B_CONSTANT_ALPHA, function);
	view->SetHighColor(0, 0, 0, alpha);
	view->DrawBitmap(bitmap, frame);
	view->SetBlendingMode(source, function);
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
