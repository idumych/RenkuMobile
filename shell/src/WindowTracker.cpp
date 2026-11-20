/*
 * WindowTracker: see WindowTracker.h.
 */
#include "WindowTracker.h"

#include <InterfaceDefs.h>
#include <Message.h>
#include <Window.h>

#include <MessengerPrivate.h>
#include <WindowInfo.h>

#include <algorithm>
#include <math.h>
#include <stdlib.h>


// An app that keeps resizing its window (a media player fitting its video)
// gets this many corrections before the shell lets it be.
static const int32 kMaxFits = 6;


WindowTracker::WindowTracker(team_id self)
	:
	fSelf(self)
{
}


bool
WindowTracker::Update()
{
	int32* tokens = NULL;
	int32 count = 0;
	if (BPrivate::get_window_order(current_workspace(), &tokens, &count)
			!= B_OK) {
		return false;
	}

	for (TrackedWindow& window : fWindows)
		window.seen = false;

	bool changed = false;
	std::vector<TrackedWindow> ordered;
	for (int32 i = 0; i < count; i++) {
		client_window_info* info = get_window_info(tokens[i]);
		if (info == NULL)
			continue;

		// Same test as Deskbar's window menu, minus our own windows.
		bool wanted = info->team != fSelf
			&& info->feel == B_NORMAL_WINDOW_FEEL
			&& info->show_hide_level <= 0;
		if (!wanted) {
			free(info);
			continue;
		}

		BRect frame(info->window_left, info->window_top, info->window_right,
			info->window_bottom);
		TrackedWindow* known = _Find(info->server_token);
		TrackedWindow window;
		if (known != NULL) {
			window = *known;
			known->seen = true;
			if (window.title != info->name || window.minimized != info->is_mini)
				changed = true;
		} else {
			window.token = info->server_token;
			window.team = info->team;
			window.fits = 0;
			BMessenger::Private(window.messenger).SetTo(info->team,
				info->client_port, info->client_token);
			changed = true;
		}
		window.title = info->name;
		window.minimized = info->is_mini;
		window.resizable = (info->flags & B_NOT_RESIZABLE) == 0;
		window.frame = frame;
		window.seen = true;
		free(info);

		if (known == NULL)
			_Fit(window, true);
		else if (!window.minimized && window.frame != _TargetFrame(window))
			_Fit(window, false);

		ordered.push_back(window);
	}
	free(tokens);

	if (ordered.size() != fWindows.size())
		changed = true;
	else {
		for (size_t i = 0; i < ordered.size(); i++) {
			if (ordered[i].token != fWindows[i].token)
				changed = true;
		}
	}

	fWindows = ordered;
	return changed;
}


void
WindowTracker::SetWorkArea(BRect area)
{
	if (area == fWorkArea)
		return;

	fWorkArea = area;
	for (TrackedWindow& window : fWindows) {
		window.fits = 0;
		_Fit(window, true);
	}
}


const TrackedWindow*
WindowTracker::Front() const
{
	for (const TrackedWindow& window : fWindows) {
		if (!window.minimized)
			return &window;
	}
	return NULL;
}


void
WindowTracker::Activate(int32 token)
{
	do_window_action(token, B_BRING_TO_FRONT, BRect(), false);
}


void
WindowTracker::Close(int32 token)
{
	TrackedWindow* window = _Find(token);
	if (window != NULL)
		window->messenger.SendMessage(B_QUIT_REQUESTED);
}


void
WindowTracker::MinimizeAll()
{
	for (const TrackedWindow& window : fWindows) {
		if (!window.minimized)
			do_window_action(window.token, B_MINIMIZE_WINDOW, BRect(), false);
	}
}


void
WindowTracker::_Fit(TrackedWindow& window, bool force)
{
	if (!fWorkArea.IsValid() || (!force && window.fits >= kMaxFits))
		return;

	window.fits++;
	if (window.resizable) {
		BMessage look(B_SET_PROPERTY);
		look.AddSpecifier("Look");
		look.AddInt32("data", B_NO_BORDER_WINDOW_LOOK);
		window.messenger.SendMessage(&look, (BHandler*)NULL, 100000);
	}

	BMessage frame(B_SET_PROPERTY);
	frame.AddSpecifier("Frame");
	frame.AddRect("data", _TargetFrame(window));
	window.messenger.SendMessage(&frame, (BHandler*)NULL, 100000);
}


BRect
WindowTracker::_TargetFrame(const TrackedWindow& window) const
{
	if (window.resizable)
		return fWorkArea;

	// Fixed-size windows (alerts, About boxes, small tools) keep their size
	// and are centred, or pinned to the top-left when they don't fit.
	BRect frame = window.frame;
	float left = fWorkArea.left
		+ std::max(0.0f, (fWorkArea.Width() - frame.Width()) / 2);
	float top = fWorkArea.top
		+ std::max(0.0f, (fWorkArea.Height() - frame.Height()) / 2);
	frame.OffsetTo(floorf(left), floorf(top));
	return frame;
}


TrackedWindow*
WindowTracker::_Find(int32 token)
{
	for (TrackedWindow& window : fWindows) {
		if (window.token == token)
			return &window;
	}
	return NULL;
}
