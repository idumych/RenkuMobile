/*
 * Renku touch filter: an input_server filter add-on that makes a finger press
 * land where the finger is.
 *
 * The tablet add-on, which every touch screen goes through, sends a press
 * before the move to where it happened. app_server takes the position of a
 * press from the last move, so with a mouse or a pen that hovers first this
 * doesn't show; but a finger doesn't hover, and its press lands where the last
 * finger left, then jumps. Taps become drags and drags start from the wrong
 * place. This sends the move first.
 *
 * A touch screen also presses with its tip switch, not a button, and the HID
 * driver only counts clicks for buttons: every press says "clicks" 0, and
 * WebKit doesn't send a click for those, so links don't open. This counts
 * them.
 *
 * A finger has no second button either, so holding it still on an app's
 * window right-clicks there, as on other touch systems. The press is held
 * back until the finger moves or lifts, which then sends it as it was; if
 * neither happens in time, a right-click goes instead and the rest of the
 * touch is dropped. A tap that was held back to its end goes out with its
 * release a moment after the press: menus, among others, poll for the
 * buttons, and miss a press that is already over. The shell's own windows
 * get their presses at once: its keyboard and panels react to a press and
 * have no use for a right-click. While a press is held back the shell shows
 * a ring filling up under the finger, until the right-click.
 *
 * shortcut: belongs in Renku's tablet add-on, HID driver and app_server; drop
 * the move and the clicks once they take the position of a press from the
 * press and count the clicks of a tip switch. The long press stays.
 */
#include "../vkbd/VirtualKeyboard.h"

#include <Autolock.h>
#include <Input.h>
#include <InputServerDevice.h>
#include <InputServerFilter.h>
#include <InterfaceDefs.h>
#include <List.h>
#include <Locker.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <Roster.h>
#include <View.h>

#include <WindowInfo.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Renku's default; asking input_server for its setting from inside one of its
// filters could deadlock.
static const bigtime_t kClickSpeed = 500000;
// Held this long without moving, a finger right-clicks.
static const bigtime_t kLongPress = 500000;
// How long a held back tap stays pressed: longer than BMenu's 50 ms polls.
static const bigtime_t kTapLength = 100000;
// The shell's tap slop: a finger that moves less than this hasn't moved.
static const float kTapSlop = 20;
// Marks the events this sends late, which come back through the filters.
static const char* kLateField = "renku:touch";


class TouchFilter : public BInputServerFilter {
public:
								TouchFilter();
	virtual						~TouchFilter();

	virtual	status_t			InitCheck();
	virtual	filter_result		Filter(BMessage* message, BList* outList);

private:
	static	status_t			_Watcher(void* data);
			void				_Tick();
			void				_Enqueue(BMessage* message);
			void				_TellShell();
			bool				_OverShell(BPoint where);

			// Never started: it's only here to enqueue events from the
			// watcher thread.
			BInputServerDevice	fInjector;

			BLocker				fLock;
			sem_id				fWake;
			thread_id			fWatcher;
			int32				fClicks = 0;
			bigtime_t			fLastPress = 0;

			// the press being held back, and the right-click it would become
			BMessage*			fHeld = NULL;
			BPoint				fHeldWhere;
			bigtime_t			fHeldSince = 0;
			bool				fLongPressed = false;

			// the release of a held back tap, and when it goes
			BMessage*			fRelease = NULL;
			bigtime_t			fReleaseAt = 0;
};


TouchFilter::TouchFilter()
	:
	fLock("renku touch")
{
	fWake = create_sem(0, "renku touch wake");
	fWatcher = spawn_thread(_Watcher, "renku touch timer",
		B_URGENT_DISPLAY_PRIORITY, this);
	if (fWake >= 0 && fWatcher >= 0)
		resume_thread(fWatcher);
}


TouchFilter::~TouchFilter()
{
	delete_sem(fWake);
	if (fWatcher >= 0) {
		status_t result;
		wait_for_thread(fWatcher, &result);
	}
	delete fHeld;
	delete fRelease;
}


status_t
TouchFilter::InitCheck()
{
	return fWake >= 0 && fWatcher >= 0 ? B_OK : B_ERROR;
}


filter_result
TouchFilter::Filter(BMessage* message, BList* outList)
{
	int32 subtype;
	if (message->FindInt32("be:device_subtype", &subtype) != B_OK
		|| subtype != B_TABLET_POINTING_DEVICE
		|| message->HasBool(kLateField)) {
		return B_DISPATCH_MESSAGE;
	}

	BPoint where;
	if (message->FindPoint("where", &where) != B_OK)
		return B_DISPATCH_MESSAGE;

	BAutolock _(fLock);

	switch (message->what) {
		case B_MOUSE_DOWN:
		{
			// a new tap can't wait for the last one's release
			if (fRelease != NULL) {
				outList->AddItem(fRelease);
				fRelease = NULL;
			}

			bigtime_t when = message->GetInt64("when", system_time());
			if (message->GetInt32("clicks", 0) == 0) {
				fClicks = when - fLastPress < kClickSpeed ? fClicks + 1 : 1;
				message->SetInt32("clicks", fClicks);
			}
			fLastPress = when;

			BMessage* moved = new BMessage(B_MOUSE_MOVED);
			moved->AddInt64("when", when);
			moved->AddInt32("buttons", 0);
			moved->AddPoint("where", where);
			moved->AddInt32("be:device_subtype", subtype);
			outList->AddItem(moved);

			if (fHeld == NULL && !fLongPressed
				&& message->GetInt32("buttons", 0) == B_PRIMARY_MOUSE_BUTTON
				&& !_OverShell(where)) {
				fHeld = new BMessage(*message);
				fHeldWhere = where;
				fHeldSince = system_time();
				release_sem(fWake);
				_TellShell();
			} else
				outList->AddItem(new BMessage(*message));
			return B_DISPATCH_MESSAGE;
		}

		case B_MOUSE_MOVED:
			if (fLongPressed)
				return B_SKIP_MESSAGE;
			if (fHeld == NULL)
				return B_DISPATCH_MESSAGE;
			if (fabsf(where.x - fHeldWhere.x) < kTapSlop
				&& fabsf(where.y - fHeldWhere.y) < kTapSlop) {
				return B_SKIP_MESSAGE;
			}
			outList->AddItem(fHeld);
			fHeld = NULL;
			_TellShell();
			outList->AddItem(new BMessage(*message));
			return B_DISPATCH_MESSAGE;

		case B_MOUSE_UP:
			if (fLongPressed) {
				fLongPressed = false;
				return B_SKIP_MESSAGE;
			}
			if (fHeld == NULL)
				return B_DISPATCH_MESSAGE;
			outList->AddItem(fHeld);
			fHeld = NULL;
			_TellShell();
			fRelease = new BMessage(*message);
			fRelease->AddBool(kLateField, true);
			fReleaseAt = system_time() + kTapLength;
			release_sem(fWake);
			return B_DISPATCH_MESSAGE;
	}

	return B_DISPATCH_MESSAGE;
}


/*!	Sleeps until the held back tap's release or the held press's long press
	is due, or until the filter changes them, and sends what is due.
*/
status_t
TouchFilter::_Watcher(void* data)
{
	TouchFilter* filter = (TouchFilter*)data;
	while (true) {
		filter->fLock.Lock();
		bigtime_t deadline = B_INFINITE_TIMEOUT;
		if (filter->fRelease != NULL)
			deadline = filter->fReleaseAt;
		else if (filter->fHeld != NULL)
			deadline = filter->fHeldSince + kLongPress;
		filter->fLock.Unlock();

		if (acquire_sem_etc(filter->fWake, 1, B_ABSOLUTE_TIMEOUT, deadline)
				== B_BAD_SEM_ID) {
			return B_OK;
		}

		filter->fLock.Lock();
		filter->_Tick();
		filter->fLock.Unlock();
	}
}


/*!	Sends whatever is due. Called with the lock held. */
void
TouchFilter::_Tick()
{
	bigtime_t now = system_time();

	if (fRelease != NULL && now >= fReleaseAt) {
		_Enqueue(fRelease);
		fRelease = NULL;
	}

	if (fHeld != NULL && now >= fHeldSince + kLongPress) {
		// the right-click
		BMessage* down = fHeld;
		fHeld = NULL;
		fLongPressed = true;
		down->SetInt64("when", now);
		down->SetInt32("buttons", B_SECONDARY_MOUSE_BUTTON);
		down->SetInt32("clicks", 1);
		down->AddBool(kLateField, true);
		_Enqueue(down);

		BMessage* up = new BMessage(B_MOUSE_UP);
		up->AddInt64("when", now);
		up->AddInt32("buttons", 0);
		up->AddPoint("where", fHeldWhere);
		up->AddInt32("be:device_subtype", B_TABLET_POINTING_DEVICE);
		up->AddBool(kLateField, true);
		_Enqueue(up);
		_TellShell();
	}
}


void
TouchFilter::_Enqueue(BMessage* message)
{
	message->SetInt64("when", system_time());
	if (fInjector.EnqueueMessage(message) != B_OK)
		delete message;
}


/*!	Tells the shell whether a press is held back, for its ring. Called with
	the lock held.
*/
void
TouchFilter::_TellShell()
{
	BMessage notice(kMsgTouchHold);
	if (fHeld != NULL) {
		notice.AddPoint("where", fHeldWhere);
		notice.AddInt64("since", fHeldSince);
		notice.AddInt64("until", fHeldSince + kLongPress);
	}
	// Input can't wait for the shell.
	BMessenger(SHELL_SIGNATURE).SendMessage(&notice, (BHandler*)NULL, 0);
}


/*!	Whether the window under \a where is one of the shell's. */
bool
TouchFilter::_OverShell(BPoint where)
{
	app_info shell;
	if (be_roster->GetAppInfo(SHELL_SIGNATURE, &shell) != B_OK)
		return false;

	int32* tokens = NULL;
	int32 count = 0;
	if (BPrivate::get_window_order(current_workspace(), &tokens, &count)
			!= B_OK) {
		return false;
	}

	// front to back: the first visible window there is the one under it
	bool overShell = false;
	for (int32 i = 0; i < count; i++) {
		client_window_info* info = get_window_info(tokens[i]);
		if (info == NULL)
			continue;

		BRect frame(info->window_left, info->window_top, info->window_right,
			info->window_bottom);
		// Presses go through the ring, as through every clear pixel of a
		// transparent window.
		bool under = info->show_hide_level <= 0 && !info->is_mini
			&& frame.Contains(where)
			&& !(info->team == shell.team
				&& strcmp(info->name, TOUCH_RING_NAME) == 0);
		if (under)
			overShell = info->team == shell.team;
		free(info);
		if (under)
			break;
	}
	free(tokens);
	return overShell;
}


extern "C" BInputServerFilter*
instantiate_input_filter()
{
	return new TouchFilter();
}
