/*
 * TouchRing: a ring that fills up under a finger held still, until the long
 * press right-clicks, as Windows does. The touch filter says when a hold
 * starts and ends (kMsgTouchHold).
 *
 * The window is transparent and its middle is clear, so on a composited
 * desktop the press goes through it to the app below. Without a compositor it
 * would catch the press, so it doesn't show.
 */
#ifndef TOUCH_RING_H
#define TOUCH_RING_H

#include <Window.h>


class BBitmap;
class BMessageRunner;
class BView;


class TouchRingWindow : public BWindow {
public:
								TouchRingWindow();
	virtual						~TouchRingWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Begin(BPoint where, bigtime_t since,
									bigtime_t until);
			void				_End();
			void				_Render();

			BView*				fView;
			BBitmap*			fBuffer;
			BView*				fBufferView;
			BMessageRunner*		fAnimation;
			bigtime_t			fSince;
			bigtime_t			fUntil;
};


#endif	// TOUCH_RING_H
