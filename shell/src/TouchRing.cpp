/*
 * TouchRing: see TouchRing.h.
 */
#include "TouchRing.h"

#include "Theme.h"

#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <MessageRunner.h>
#include <View.h>

#include <algorithm>
#include <string.h>


static const float kSize = 76;
static const float kPenSize = 6;
// A tap is over before this, so the ring only shows for a hold.
static const bigtime_t kDelay = 150000;


class TouchRingView : public BView {
public:
	TouchRingView(BRect frame, BBitmap* buffer)
		:
		BView(frame, "ring", B_FOLLOW_ALL, B_WILL_DRAW),
		fBuffer(buffer)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	virtual void Draw(BRect updateRect)
	{
		DrawBitmap(fBuffer, updateRect, updateRect);
	}

private:
	BBitmap*	fBuffer;
};


TouchRingWindow::TouchRingWindow()
	:
	BWindow(BRect(0, 0, kSize - 1, kSize - 1), TOUCH_RING_NAME,
		B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
		B_NOT_MOVABLE | B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
			| B_NOT_CLOSABLE | B_AVOID_FOCUS | B_ASYNCHRONOUS_CONTROLS
			| B_TRANSPARENT_WINDOW,
		B_ALL_WORKSPACES),
	fAnimation(NULL),
	fSince(0),
	fUntil(0)
{
	fBuffer = new BBitmap(Bounds(), B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
	fBufferView = new BView(fBuffer->Bounds(), "buffer", B_FOLLOW_NONE, 0);
	fBuffer->AddChild(fBufferView);

	fView = new TouchRingView(Bounds(), fBuffer);
	AddChild(fView);
}


TouchRingWindow::~TouchRingWindow()
{
	delete fAnimation;
	delete fBuffer;
}


void
TouchRingWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgTouchHold:
		{
			BPoint where;
			if (message->FindPoint("where", &where) == B_OK) {
				_Begin(where, message->GetInt64("since", 0),
					message->GetInt64("until", 0));
			} else
				_End();
			break;
		}

		case kMsgAnimate:
			_Render();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
TouchRingWindow::_Begin(BPoint where, bigtime_t since, bigtime_t until)
{
	// app_server only answers this when it starts, so ask once.
	static const bool sComposited = is_composited_desktop();
	if (!sComposited || until <= since + kDelay)
		return;

	fSince = since;
	fUntil = until;
	MoveTo(where.x - kSize / 2, where.y - kSize / 2);

	delete fAnimation;
	BMessage tick(kMsgAnimate);
	fAnimation = new BMessageRunner(BMessenger(this), &tick, 16000);
}


void
TouchRingWindow::_End()
{
	delete fAnimation;
	fAnimation = NULL;
	if (!IsHidden())
		Hide();
}


void
TouchRingWindow::_Render()
{
	bigtime_t now = system_time();
	if (now < fSince + kDelay)
		return;
	float progress = std::min(1.0f,
		(float)(now - fSince - kDelay) / (fUntil - fSince - kDelay));

	if (!fBuffer->Lock())
		return;

	// Clear, so that the press goes through the middle; copy fills always
	// draw opaque.
	memset(fBuffer->Bits(), 0, fBuffer->BitsLength());

	BView* view = fBufferView;
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
	view->SetPenSize(kPenSize);
	BRect ring = view->Bounds().InsetByCopy(kPenSize, kPenSize);

	view->SetHighColor(Theme::kScrim);
	view->StrokeEllipse(ring);
	// clockwise from the top, as a clock fills: angles run anticlockwise, so
	// the arc starts back from the top and ends there
	view->SetHighColor(Theme::kAccent);
	view->StrokeArc(ring, 90 - 360 * progress, 360 * progress);

	view->Sync();
	fBuffer->Unlock();
	fView->Invalidate();

	if (IsHidden())
		Show();
	if (progress >= 1) {
		delete fAnimation;
		fAnimation = NULL;
	}
}
