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


// A window scaled up more than this only gets blurrier.
static const float kMaxScale = 2;

// Renku's compositor scales windows with this private call; the system's
// libbe may not have it yet, so it's looked for when the shell starts.
namespace BPrivate {
status_t set_window_scale(int32 windowToken, float scale)
	__attribute__((weak));
}


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
			window.capped = false;
			window.scale = 1;
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

		// Fitted twice and still short of the area: its app caps its size
		// (DeskCalc's at 400 pixels wide), so it's centred instead. The size
		// limits aren't in the window list, so this is how they show.
		if (window.resizable && !window.capped && window.fits >= 2
			&& (frame.Width() < fWorkArea.Width() - 1
				|| frame.Height() < fWorkArea.Height() - 1)) {
			window.capped = true;
		}

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
	BMessage look(B_SET_PROPERTY);
	look.AddSpecifier("Look");
	look.AddInt32("data", B_NO_BORDER_WINDOW_LOOK);
	window.messenger.SendMessage(&look, (BHandler*)NULL, 100000);

	BMessage frame(B_SET_PROPERTY);
	frame.AddSpecifier("Frame");
	frame.AddRect("data", _TargetFrame(window));
	window.messenger.SendMessage(&frame, (BHandler*)NULL, 100000);

	float scale = _ScaleFor(window);
	if (scale != window.scale) {
		BPrivate::set_window_scale(window.token, scale);
		window.scale = scale;
	}
}


/*!	How much a window that won't fill the work area is scaled to fit it: as
	far as it can grow in both directions, up to kMaxScale. 1 when it fills
	the area, or when the compositor can't scale.
*/
float
WindowTracker::_ScaleFor(const TrackedWindow& window) const
{
	if ((window.resizable && !window.capped)
		|| BPrivate::set_window_scale == NULL) {
		return 1;
	}
	float scale = std::min((fWorkArea.Width() + 1) / (window.frame.Width() + 1),
		(fWorkArea.Height() + 1) / (window.frame.Height() + 1));
	return std::clamp(scale, 0.25f, kMaxScale);
}


BRect
WindowTracker::_TargetFrame(const TrackedWindow& window) const
{
	if (window.resizable && !window.capped)
		return fWorkArea;

	// Fixed-size windows (alerts, About boxes, small tools) and capped ones
	// keep their size, and what's shown of them, scaled, is centred, or
	// pinned to the top-left when it doesn't fit.
	BRect frame = window.frame;
	float scale = _ScaleFor(window);
	float left = fWorkArea.left + std::max(0.0f,
		(fWorkArea.Width() + 1 - (frame.Width() + 1) * scale) / 2);
	float top = fWorkArea.top + std::max(0.0f,
		(fWorkArea.Height() + 1 - (frame.Height() + 1) * scale) / 2);
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
