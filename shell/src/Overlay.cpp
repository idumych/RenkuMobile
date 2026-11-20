/*
 * Overlay: see Overlay.h.
 */
#include "Overlay.h"

#include "Theme.h"

#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <MessageRunner.h>
#include <Screen.h>

#include <algorithm>
#include <string.h>


static const float kAnimationStep = 0.25f;


// app_server only answers this when it starts, so ask once.
static bool
composited()
{
	static const bool sComposited = is_composited_desktop();
	return sComposited;
}


OverlayView::OverlayView(BRect frame, const char* name)
	:
	BView(frame, name, B_FOLLOW_ALL, B_WILL_DRAW),
	fBackdrop(NULL),
	fAnimation(NULL),
	fProgress(0),
	fOpening(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	fBuffer = new BBitmap(frame.OffsetToCopy(0, 0), B_BITMAP_ACCEPTS_VIEWS,
		composited() ? B_RGBA32 : B_RGB32);
	fBufferView = new BView(fBuffer->Bounds(), "buffer", B_FOLLOW_NONE, 0);
	fBuffer->AddChild(fBufferView);
}


OverlayView::~OverlayView()
{
	delete fAnimation;
	delete fBackdrop;
	delete fBuffer;
}


void
OverlayView::Draw(BRect updateRect)
{
	DrawBitmap(fBuffer, updateRect, updateRect);
}


void
OverlayView::MessageReceived(BMessage* message)
{
	if (message->what != kMsgAnimate) {
		BView::MessageReceived(message);
		return;
	}

	fProgress = std::clamp(fProgress + (fOpening ? kAnimationStep
		: -kAnimationStep), 0.0f, 1.0f);
	Render();
	if ((fOpening && fProgress >= 1) || (!fOpening && fProgress <= 0)) {
		delete fAnimation;
		fAnimation = NULL;
		AnimationDone(fOpening);
	}
}


void
OverlayView::Prepare()
{
	delete fBackdrop;
	fBackdrop = NULL;
	if (!composited())
		BScreen().GetBitmap(&fBackdrop, false);
}


void
OverlayView::Render()
{
	if (!fBuffer->Lock())
		return;

	BView* view = fBufferView;
	BRect bounds = view->Bounds();
	// On a composited desktop the apps show through wherever the buffer is
	// clear. Copy fills always draw opaque, so clear the pixels directly.
	view->SetDrawingMode(B_OP_COPY);
	if (composited())
		memset(fBuffer->Bits(), 0, fBuffer->BitsLength());
	else if (fBackdrop != NULL)
		view->DrawBitmap(fBackdrop, ConvertToScreen(bounds), bounds);
	else {
		view->SetHighColor(Theme::kPanel);
		view->FillRect(bounds);
	}

	// Dim the screen behind, more as the overlay opens. Composite blending
	// keeps the buffer's alpha right for the compositor.
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA,
		composited() ? B_ALPHA_COMPOSITE : B_ALPHA_OVERLAY);
	rgb_color scrim = Theme::kScrim;
	scrim.alpha = (uint8)(scrim.alpha * fProgress);
	view->SetHighColor(scrim);
	view->FillRect(bounds);

	RenderContent(view);

	view->Sync();
	fBuffer->Unlock();
	Invalidate();
}


void
OverlayView::StartAnimation(bool opening)
{
	fOpening = opening;
	delete fAnimation;
	BMessage tick(kMsgAnimate);
	fAnimation = new BMessageRunner(BMessenger(this), &tick, 16000);
	Render();
}


void
OverlayView::AnimationDone(bool opened)
{
	if (!opened)
		Window()->Hide();
}


// #pragma mark - OverlayWindow


OverlayWindow::OverlayWindow(BRect frame, const char* name)
	:
	BWindow(frame, name, B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
		B_NOT_MOVABLE | B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
			| B_NOT_CLOSABLE | B_AVOID_FOCUS | B_ASYNCHRONOUS_CONTROLS
			| B_TRANSPARENT_WINDOW,
		B_ALL_WORKSPACES)
{
}


void
OverlayWindow::Open(OverlayView* view)
{
	if (!Lock())
		return;
	if (IsHidden()) {
		view->Prepare();
		view->Render();
		Show();
	}
	Unlock();
}
