/*
 * WindowTracker: the shell's window manager.
 *
 * Haiku's app_server has no pluggable window manager, so the shell polls the
 * window list (the private API Deskbar uses) and, through each window's
 * scripting suite, makes every app window borderless and sizes it to the work
 * area between the status bar and the navigation panel (and the keyboard, when
 * it's up). The task switcher and the navigation buttons work from this list.
 */
#ifndef WINDOW_TRACKER_H
#define WINDOW_TRACKER_H

#include <Messenger.h>
#include <OS.h>
#include <Rect.h>
#include <String.h>

#include <vector>


struct TrackedWindow {
	int32		token;		// app_server token
	team_id		team;
	BString		title;
	bool		minimized;
	bool		resizable;
	BMessenger	messenger;	// the BWindow, for scripting
	BRect		frame;
	int32		fits;		// how often we've had to resize it
	bool		seen;
};


class WindowTracker {
public:
								WindowTracker(team_id self);

			// Polls app_server. Returns true when the list changed.
			bool				Update();

			void				SetWorkArea(BRect area);
			BRect				WorkArea() const { return fWorkArea; }

			const std::vector<TrackedWindow>& Windows() const
									{ return fWindows; }
			const TrackedWindow* Front() const;

			void				Activate(int32 token);
			void				Close(int32 token);
			void				MinimizeAll();

private:
			void				_Fit(TrackedWindow& window, bool force);
			BRect				_TargetFrame(const TrackedWindow& window) const;
			TrackedWindow*		_Find(int32 token);

			team_id				fSelf;
			BRect				fWorkArea;
			std::vector<TrackedWindow> fWindows;	// front to back
};


#endif	// WINDOW_TRACKER_H
