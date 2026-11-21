/*
 * TaskSwitcher: a card for every open window. Tap a card to switch to it, tap
 * its close button to close it, tap anywhere else to go back.
 *
 * Where Renku's compositor offers them (get_window_preview()), the cards show
 * live previews of the windows, refreshed ten times a second while it's
 * open; a window whose buffer has gone shows how it last looked, and one that
 * has never been seen, its app's icon.
 *
 * With them it zooms, as Plasma Mobile's does: the app open when it's called
 * shrinks from the screen into its card while the rest fade in, and the card
 * picked grows back to the screen, its app in front under it. Without them,
 * the cards slide in and out.
 */
#ifndef TASK_SWITCHER_H
#define TASK_SWITCHER_H

#include "Overlay.h"
#include "WindowTracker.h"

#include <map>
#include <vector>


class BBitmap;
class BMessageRunner;


class TaskSwitcherView : public OverlayView {
public:
								TaskSwitcherView(BRect frame);
	virtual						~TaskSwitcherView();

	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);
	virtual	void				MessageReceived(BMessage* message);

			void				Open(const std::vector<TrackedWindow>& windows);
			void				Close();

protected:
	virtual	void				RenderContent(BView* view);
	virtual	void				AnimationDone(bool opened);

private:
	struct Card {
		int32		token;
		BString		title;
		BBitmap*	icon;
		BRect		frame;
		BRect		previewFrame;
		BRect		windowFrame;	// where the window is, in this view
		bool		minimized;
	};
	struct Preview {
		BBitmap*	bitmap;
		bool		shown;		// it has had the window's contents
	};

			BRect				_CloseFrame(const Card& card) const;
			void				_Clear();
			void				_RefreshPreviews();
			bool				_StartZoom(int32 card);
			void				_DrawFaded(BView* view, BBitmap* bitmap,
									BRect frame, uint8 alpha);

			std::vector<Card>	fCards;
			// kept between openings, by window token
			std::map<int32, Preview> fPreviews;
			BMessageRunner*		fRefresh;
			// the card zooming, and its window at full size, while it does
			int32				fZoomCard;
			BBitmap*			fZoom;
			int32				fPicked;	// the window to bring up after

			int32				fPressed;
			bool				fPressedClose;
};


#endif	// TASK_SWITCHER_H
