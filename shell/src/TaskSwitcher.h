/*
 * TaskSwitcher: a card for every open window. Tap a card to switch to it, tap
 * its close button to close it, tap anywhere else to go back.
 */
#ifndef TASK_SWITCHER_H
#define TASK_SWITCHER_H

#include "Overlay.h"
#include "WindowTracker.h"

#include <vector>


class BBitmap;


class TaskSwitcherView : public OverlayView {
public:
								TaskSwitcherView(BRect frame);
	virtual						~TaskSwitcherView();

	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseUp(BPoint where);

			void				Open(const std::vector<TrackedWindow>& windows);
			void				Close();

protected:
	virtual	void				RenderContent(BView* view);

private:
	struct Card {
		int32		token;
		BString		title;
		BBitmap*	icon;
		BRect		frame;
	};

			BRect				_CloseFrame(const Card& card) const;
			void				_Clear();

			std::vector<Card>	fCards;
			int32				fPressed;
			bool				fPressedClose;
};


#endif	// TASK_SWITCHER_H
