/*
 * Renku Mobile shell: the phone UI. It replaces Tracker's desktop and the
 * Deskbar with a home screen, a status bar, a navigation panel, an action
 * drawer, a task switcher and an on-screen keyboard, and keeps every app
 * window full-screen between the panels.
 */
#include "ActionDrawer.h"
#include "AppList.h"
#include "HomeScreen.h"
#include "Keyboard.h"
#include "Panels.h"
#include "TaskSwitcher.h"
#include "Theme.h"
#include "TouchRing.h"
#include "WindowTracker.h"

#include <Application.h>
#include <MessageRunner.h>
#include <Roster.h>
#include <Screen.h>

#include <stdlib.h>


static const uint32 kMsgTrackWindows = 'trck';
static const uint32 kMsgScreenshotNow = 'shtN';


class ShellApp : public BApplication {
public:
								ShellApp();
	virtual						~ShellApp();

	virtual	void				ReadyToRun();
	virtual	void				MessageReceived(BMessage* message);

private:
			void				_SetKeyboard(bool show);
			void				_UpdateWorkArea();
			void				_Launch(const entry_ref& ref);
			void				_HideOverlays();

			AppList				fApps;
			WindowTracker		fTracker;
			BRect				fScreen;

			HomeWindow*			fHome;
			PanelWindow*		fStatusBar;
			StatusBarView*		fStatusView;
			PanelWindow*		fNavBar;
			PanelWindow*		fKeyboard;
			KeyboardView*		fKeyboardView;
			OverlayWindow*		fDrawerWindow;
			ActionDrawerView*	fDrawer;
			OverlayWindow*		fTasksWindow;
			TaskSwitcherView*	fTasks;
			TouchRingWindow*	fTouchRing;

			BMessageRunner*		fPulse;
			BMessageRunner*		fTrackRunner;
};


ShellApp::ShellApp()
	:
	BApplication(SHELL_SIGNATURE),
	fTracker(be_app->Team()),
	fKeyboard(NULL),
	fKeyboardView(NULL),
	fTouchRing(NULL),
	fPulse(NULL),
	fTrackRunner(NULL)
{
}


ShellApp::~ShellApp()
{
	delete fPulse;
	delete fTrackRunner;
}


void
ShellApp::ReadyToRun()
{
	fScreen = BScreen().Frame();
	fApps.Scan();

	fHome = new HomeWindow(fScreen, &fApps);
	fHome->Show();

	BRect frame = fScreen;
	frame.bottom = frame.top + Theme::kStatusBarHeight - 1;
	fStatusBar = new PanelWindow(frame, "Status bar");
	fStatusView = new StatusBarView(fStatusBar->Bounds());
	fStatusBar->AddChild(fStatusView);
	fStatusBar->Show();

	frame = fScreen;
	frame.top = frame.bottom - Theme::kNavBarHeight + 1;
	fNavBar = new PanelWindow(frame, "Navigation");
	fNavBar->AddChild(new NavBarView(fNavBar->Bounds()));
	fNavBar->Show();

	frame.bottom = frame.top - 1;
	frame.top = frame.bottom - Theme::kKeyboardHeight + 1;
	fKeyboard = new PanelWindow(frame, "Keyboard");
	fKeyboardView = new KeyboardView(fKeyboard->Bounds());
	fKeyboard->AddChild(fKeyboardView);
	// Hide() then Show() starts a window's thread but leaves it hidden.
	fKeyboard->Hide();
	fKeyboard->Show();

	// The overlays sit below the status bar and above the navigation panel,
	// so both stay usable while one is open.
	frame = fScreen;
	frame.top += Theme::kStatusBarHeight;
	fDrawerWindow = new OverlayWindow(frame, "Action drawer");
	fDrawer = new ActionDrawerView(fDrawerWindow->Bounds());
	fDrawerWindow->AddChild(fDrawer);
	fDrawerWindow->Hide();
	fDrawerWindow->Show();

	frame.bottom -= Theme::kNavBarHeight;
	fTasksWindow = new OverlayWindow(frame, "Task switcher");
	fTasks = new TaskSwitcherView(fTasksWindow->Bounds());
	fTasksWindow->AddChild(fTasks);
	fTasksWindow->Hide();
	fTasksWindow->Show();

	fTouchRing = new TouchRingWindow();
	fTouchRing->Hide();
	fTouchRing->Show();

	_UpdateWorkArea();

	BMessage pulse(kMsgPulse);
	fPulse = new BMessageRunner(be_app_messenger, &pulse, 1000000);
	BMessage track(kMsgTrackWindows);
	fTrackRunner = new BMessageRunner(be_app_messenger, &track, 300000);
}


void
ShellApp::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgPulse:
			fStatusBar->PostMessage(kMsgPulse, fStatusView);
			fHome->PostMessage(kMsgPulse, fHome->View());
			break;

		case kMsgTrackWindows:
			fTracker.Update();
			break;

		case kMsgLaunch:
		{
			entry_ref ref;
			if (message->FindRef("ref", &ref) == B_OK)
				_Launch(ref);
			break;
		}

		case kMsgGoHome:
			_HideOverlays();
			_SetKeyboard(false);
			fTracker.MinimizeAll();
			if (fHome->Lock()) {
				fHome->View()->ShowHome();
				fHome->Unlock();
			}
			break;

		case kMsgShowSettings:
			_HideOverlays();
			fTracker.MinimizeAll();
			if (fHome->Lock()) {
				fHome->View()->ShowSettings();
				fHome->Unlock();
			}
			break;

		case kMsgShowTasks:
			if (fDrawerWindow->Lock()) {
				if (!fDrawerWindow->IsHidden())
					fDrawer->Close();
				fDrawerWindow->Unlock();
			}
			fTracker.Update();
			if (fTasksWindow->Lock()) {
				fTasks->Open(fTracker.Windows());
				fTasksWindow->Unlock();
			}
			break;

		case kMsgShowDrawer:
			if (fDrawerWindow->Lock()) {
				fDrawer->Open();
				fDrawerWindow->Unlock();
			}
			break;

		case kMsgCloseActive:
		{
			_HideOverlays();
			fTracker.Update();
			if (const TrackedWindow* front = fTracker.Front())
				fTracker.Close(front->token);
			break;
		}

		case kMsgActivateWindow:
		case kMsgCloseWindow:
		{
			int32 token;
			if (message->FindInt32("token", &token) != B_OK)
				break;
			if (message->what == kMsgActivateWindow)
				fTracker.Activate(token);
			else
				fTracker.Close(token);
			break;
		}

		case kMsgToggleKeyboard:
		{
			bool hidden = true;
			if (fKeyboard->Lock()) {
				hidden = fKeyboard->IsHidden();
				fKeyboard->Unlock();
			}
			_SetKeyboard(hidden);
			break;
		}

		case kMsgTextFocus:
			// From the input_server add-on: as in Plasma, the keyboard comes
			// up with a text field and goes with it.
			if (fKeyboard != NULL)
				_SetKeyboard(message->GetBool("text"));
			break;

		case kMsgKeyboardResized:
			_UpdateWorkArea();
			break;

		case kMsgTouchHold:
			// From the touch filter: a finger held still towards a long press.
			if (fTouchRing != NULL)
				fTouchRing->PostMessage(message);
			break;

		case kMsgTakeScreenshot:
		{
			// Wait for the drawer to slide away first.
			BMessage now(kMsgScreenshotNow);
			BMessageRunner::StartSending(be_app_messenger, &now, 500000, 1);
			break;
		}

		case kMsgScreenshotNow:
			system("/bin/screenshot --silent &");
			break;

		default:
			BApplication::MessageReceived(message);
	}
}


void
ShellApp::_SetKeyboard(bool show)
{
	if (fKeyboard->Lock()) {
		if (show && fKeyboard->IsHidden())
			fKeyboard->Show();
		else if (!show && !fKeyboard->IsHidden()) {
			fKeyboardView->CloseMenu();
			fKeyboard->Hide();
		}
		fKeyboard->Unlock();
	}
	if (fDrawerWindow->Lock()) {
		fDrawer->SetKeyboardShown(show);
		fDrawerWindow->Unlock();
	}
	_UpdateWorkArea();
}


void
ShellApp::_UpdateWorkArea()
{
	BRect area = fScreen;
	area.top += Theme::kStatusBarHeight;
	area.bottom -= Theme::kNavBarHeight;
	if (fKeyboard->Lock()) {
		// as tall as its layout makes it
		if (!fKeyboard->IsHidden())
			area.bottom -= fKeyboard->Frame().Height() + 1;
		fKeyboard->Unlock();
	}
	fTracker.SetWorkArea(area);
}


void
ShellApp::_Launch(const entry_ref& ref)
{
	// Single-launch apps that already run: bring their newest window forward.
	team_id team;
	if (be_roster->Launch(&ref, (BMessage*)NULL, &team) != B_ALREADY_RUNNING)
		return;
	fTracker.Update();
	for (const TrackedWindow& window : fTracker.Windows()) {
		if (window.team == team) {
			fTracker.Activate(window.token);
			return;
		}
	}
	be_roster->ActivateApp(team);
}


void
ShellApp::_HideOverlays()
{
	if (fDrawerWindow->Lock()) {
		if (!fDrawerWindow->IsHidden())
			fDrawer->Close();
		fDrawerWindow->Unlock();
	}
	if (fTasksWindow->Lock()) {
		if (!fTasksWindow->IsHidden())
			fTasks->Close();
		fTasksWindow->Unlock();
	}
}


int
main()
{
	ShellApp app;
	app.Run();
	return 0;
}
