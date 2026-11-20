/*
 * Overlay: a full-screen shell window that shows over the apps, such as the
 * action drawer and the task switcher.
 *
 * app_server doesn't composite, so an overlay can't be see-through. Instead it
 * takes a picture of the screen as it opens and draws that, dimmed, behind its
 * own content. Everything is drawn into an off-screen bitmap first so that
 * dragging and the slide-in don't flicker.
 */
#ifndef OVERLAY_H
#define OVERLAY_H

#include <View.h>
#include <Window.h>


class BBitmap;
class BMessageRunner;


class OverlayView : public BView {
public:
								OverlayView(BRect frame, const char* name);
	virtual						~OverlayView();

	virtual	void				Draw(BRect updateRect);
	virtual	void				MessageReceived(BMessage* message);

			// Captures the screen; call before the window shows.
			void				Prepare();
			void				Render();

protected:
	virtual	void				RenderContent(BView* view) = 0;

			// 0 = closed, 1 = fully open; animated by StartAnimation().
			float				Progress() const { return fProgress; }
			void				StartAnimation(bool opening);
	virtual	void				AnimationDone(bool opened);

			const BBitmap*		Backdrop() const { return fBackdrop; }

private:
			BBitmap*			fBackdrop;
			BBitmap*			fBuffer;
			BView*				fBufferView;
			BMessageRunner*		fAnimation;
			float				fProgress;
			bool				fOpening;
};


class OverlayWindow : public BWindow {
public:
								OverlayWindow(BRect frame, const char* name);

			// Shows the window, capturing the screen first.
			void				Open(OverlayView* view);
};


#endif	// OVERLAY_H
