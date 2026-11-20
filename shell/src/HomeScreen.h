/*
 * HomeScreen: the window behind every app. A clock, a grid of favourite apps
 * and a dock over the wallpaper; swipe up (or tap the handle) for the app
 * drawer with every app, swipe down to close it again. The drawer scrolls with
 * the finger and keeps going after a flick.
 */
#ifndef HOME_SCREEN_H
#define HOME_SCREEN_H

#include <View.h>
#include <Window.h>

#include <vector>


class AppList;
struct AppEntry;
class BBitmap;
class BMessageRunner;


class HomeView : public BView {
public:
								HomeView(BRect frame, AppList* apps);
	virtual						~HomeView();

	virtual	void				AttachedToWindow();
	virtual	void				FrameResized(float width, float height);
	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseUp(BPoint where);
	virtual	void				MessageReceived(BMessage* message);

			void				ShowHome();
			void				ShowSettings();

private:
	struct Cell {
		BRect		frame;
		AppEntry*	app;
		bool		label;
	};

			void				_MakeWallpaper();
			void				_Layout();
			void				_Render();
			void				_RenderCell(BView* view, const Cell& cell,
									float offset);
			void				_RenderClock(BView* view);
			void				_SetDrawer(bool open, bool settings = false);
			float				_MaxScroll() const;
			const Cell*			_CellAt(BPoint where) const;
			void				_StopFling();
			void				_Fling();

			AppList*			fApps;
			BBitmap*			fWallpaper;
			BBitmap*			fBuffer;
			BView*				fBufferView;

			std::vector<Cell>	fHomeCells;
			std::vector<Cell>	fDrawerCells;
			BRect				fDock;
			BRect				fHandle;

			bool				fDrawerOpen;
			bool				fSettingsOnly;
			float				fScroll;
			float				fScrollAtDown;
			bool				fTracking;
			bool				fMoved;
			BPoint				fDownPoint;
			const Cell*			fPressedCell;

			// The finger's speed in pixels per second, and the scrolling it
			// keeps up after a flick
			float				fVelocity;
			BPoint				fLastPoint;
			bigtime_t			fLastTime;
			BMessageRunner*		fFling;
			bigtime_t			fFlingTime;
};


class HomeWindow : public BWindow {
public:
								HomeWindow(BRect frame, AppList* apps);

			HomeView*			View() const { return fView; }

private:
			HomeView*			fView;
};


#endif	// HOME_SCREEN_H
