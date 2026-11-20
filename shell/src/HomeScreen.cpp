/*
 * HomeScreen: see HomeScreen.h.
 */
#include "HomeScreen.h"

#include "AppList.h"
#include "Glyphs.h"
#include "Theme.h"

#include <Application.h>
#include <Bitmap.h>
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <Region.h>
#include <Shape.h>

#include <WindowPrivate.h>

#include <algorithm>
#include <math.h>
#include <time.h>


static const float kTapSlop = 14;
static const float kDrawerSwipe = 70;
static const float kClockTop = Theme::kStatusBarHeight + 70;
static const float kGridTop = 380;
static const float kDrawerTop = Theme::kStatusBarHeight + 96;


static rgb_color
with_alpha(rgb_color color, uint8 alpha)
{
	color.alpha = alpha;
	return color;
}


HomeView::HomeView(BRect frame, AppList* apps)
	:
	BView(frame, "home", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS),
	fApps(apps),
	fWallpaper(NULL),
	fBuffer(NULL),
	fBufferView(NULL),
	fDrawerOpen(false),
	fSettingsOnly(false),
	fScroll(0),
	fScrollAtDown(0),
	fTracking(false),
	fMoved(false),
	fPressedCell(NULL)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}


HomeView::~HomeView()
{
	delete fWallpaper;
	delete fBuffer;
}


void
HomeView::AttachedToWindow()
{
	FrameResized(Bounds().Width(), Bounds().Height());
}


void
HomeView::FrameResized(float width, float height)
{
	delete fBuffer;
	fBuffer = new BBitmap(Bounds(), B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
	fBufferView = new BView(Bounds(), "buffer", B_FOLLOW_NONE, 0);
	fBuffer->AddChild(fBufferView);

	_MakeWallpaper();
	_Layout();
	_Render();
}


void
HomeView::Draw(BRect updateRect)
{
	if (fBuffer != NULL)
		DrawBitmap(fBuffer, updateRect, updateRect);
}


void
HomeView::MouseDown(BPoint where)
{
	fTracking = true;
	fMoved = false;
	fDownPoint = where;
	fScrollAtDown = fScroll;
	fPressedCell = _CellAt(where);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	if (fPressedCell != NULL)
		_Render();
}


void
HomeView::MouseMoved(BPoint where, uint32 transit, const BMessage* dragMessage)
{
	if (!fTracking)
		return;

	float dy = where.y - fDownPoint.y;
	if (!fMoved && fabsf(dy) < kTapSlop && fabsf(where.x - fDownPoint.x) < kTapSlop)
		return;

	if (!fMoved) {
		fMoved = true;
		fPressedCell = NULL;
	}

	if (!fDrawerOpen) {
		if (dy < -kDrawerSwipe) {
			fTracking = false;
			_SetDrawer(true);
		}
		return;
	}

	// In the drawer a drag scrolls; pulling down from the top closes it.
	if (fScrollAtDown <= 0 && dy > kDrawerSwipe) {
		fTracking = false;
		_SetDrawer(false);
		return;
	}
	float scroll = std::clamp(fScrollAtDown - dy, 0.0f, _MaxScroll());
	if (scroll != fScroll) {
		fScroll = scroll;
		_Render();
	}
}


void
HomeView::MouseUp(BPoint where)
{
	const Cell* pressed = fPressedCell;
	bool tap = fTracking && !fMoved;
	fTracking = false;
	fPressedCell = NULL;

	if (!tap) {
		_Render();
		return;
	}

	if (pressed != NULL && pressed == _CellAt(where)) {
		BMessage launch(kMsgLaunch);
		launch.AddRef("ref", &pressed->app->ref);
		be_app->PostMessage(&launch);
		if (fDrawerOpen)
			_SetDrawer(false);
		else
			_Render();
		return;
	}

	if (!fDrawerOpen && fHandle.Contains(where))
		_SetDrawer(true);
	else
		_Render();
}


void
HomeView::MessageReceived(BMessage* message)
{
	if (message->what == kMsgPulse) {
		// Redraw when the minute changes.
		static int lastMinute = -1;
		time_t now = time(NULL);
		struct tm local;
		localtime_r(&now, &local);
		if (local.tm_min != lastMinute) {
			lastMinute = local.tm_min;
			if (!fDrawerOpen)
				_Render();
		}
		return;
	}
	BView::MessageReceived(message);
}


void
HomeView::ShowHome()
{
	if (fDrawerOpen)
		_SetDrawer(false);
}


void
HomeView::ShowSettings()
{
	_SetDrawer(true, true);
}


void
HomeView::_MakeWallpaper()
{
	delete fWallpaper;
	fWallpaper = NULL;
	BRect bounds = Bounds();

	// A night-blue gradient with soft ridges, after Plasma's default
	// wallpapers.
	fWallpaper = new BBitmap(bounds, B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
	BView* view = new BView(bounds, "wallpaper", B_FOLLOW_NONE, 0);
	fWallpaper->AddChild(view);
	fWallpaper->Lock();

	BGradientLinear sky(BPoint(0, 0), BPoint(0, bounds.bottom));
	sky.AddColor(make_color(14, 20, 38), 0);
	sky.AddColor(make_color(22, 52, 92), 130);
	sky.AddColor(make_color(28, 104, 140), 255);
	view->FillRect(bounds, sky);

	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	BGradientRadial glow(BPoint(bounds.Width() * 0.7f, bounds.Height() * 0.32f),
		bounds.Width() * 0.8f);
	glow.AddColor(make_color(61, 174, 233, 90), 0);
	glow.AddColor(make_color(61, 174, 233, 0), 255);
	view->FillRect(bounds, glow);

	struct Ridge { float base, amplitude, phase; rgb_color color; };
	const Ridge ridges[] = {
		{0.62f, 0.07f, 0.4f, make_color(36, 120, 170, 70)},
		{0.72f, 0.06f, 2.1f, make_color(20, 70, 120, 110)},
		{0.82f, 0.05f, 4.0f, make_color(10, 30, 60, 160)}
	};
	for (const Ridge& ridge : ridges) {
		BShape shape;
		shape.MoveTo(BPoint(0, bounds.bottom));
		for (int x = 0; x <= 24; x++) {
			float fx = x / 24.0f;
			float y = ridge.base + ridge.amplitude * sinf(fx * 5.2f + ridge.phase)
				* (0.6f + 0.4f * sinf(fx * 11 + ridge.phase * 2));
			shape.LineTo(BPoint(fx * bounds.Width(), y * bounds.Height()));
		}
		shape.LineTo(BPoint(bounds.right, bounds.bottom));
		shape.Close();
		view->SetHighColor(ridge.color);
		view->FillShape(&shape);
	}

	view->Sync();
	fWallpaper->Unlock();
}


void
HomeView::_Layout()
{
	BRect bounds = Bounds();
	float cellWidth = floorf(bounds.Width() / Theme::kGridColumns);
	float bottom = bounds.bottom - Theme::kNavBarHeight;

	fDock = BRect(24, bottom - 148, bounds.right - 24, bottom - 24);
	fHandle = BRect(bounds.Width() / 2 - 40, fDock.top - 64,
		bounds.Width() / 2 + 40, fDock.top - 12);

	fHomeCells.clear();
	const std::vector<AppEntry*>& favorites = fApps->Favorites();
	int32 rows = (int32)((fHandle.top - kGridTop) / Theme::kCellHeight);
	for (size_t i = 0; i < favorites.size()
			&& (int32)i < rows * Theme::kGridColumns; i++) {
		int32 column = i % Theme::kGridColumns;
		int32 row = i / Theme::kGridColumns;
		BRect frame(0, 0, cellWidth - 1, Theme::kCellHeight - 1);
		frame.OffsetTo(column * cellWidth, kGridTop + row * Theme::kCellHeight);
		fHomeCells.push_back({frame, favorites[i], true});
	}

	const std::vector<AppEntry*>& dock = fApps->Dock();
	float dockCell = fDock.Width() / std::max<size_t>(4, dock.size());
	for (size_t i = 0; i < dock.size(); i++) {
		BRect frame(fDock.left + i * dockCell, fDock.top,
			fDock.left + (i + 1) * dockCell - 1, fDock.bottom);
		fHomeCells.push_back({frame, dock[i], false});
	}

	fDrawerCells.clear();
	for (AppEntry* app : fApps->Apps()) {
		if (fSettingsOnly && !app->preference)
			continue;
		int32 i = fDrawerCells.size();
		int32 column = i % Theme::kGridColumns;
		int32 row = i / Theme::kGridColumns;
		BRect frame(0, 0, cellWidth - 1, Theme::kCellHeight - 1);
		frame.OffsetTo(column * cellWidth, kDrawerTop + row * Theme::kCellHeight);
		fDrawerCells.push_back({frame, app, true});
	}
}


void
HomeView::_Render()
{
	if (fBuffer == NULL || !fBuffer->Lock())
		return;

	BView* view = fBufferView;
	BRect bounds = view->Bounds();
	view->SetDrawingMode(B_OP_COPY);
	view->DrawBitmap(fWallpaper, bounds);

	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	if (!fDrawerOpen) {
		_RenderClock(view);

		view->SetHighColor(with_alpha(Theme::kPanel, 110));
		view->FillRoundRect(fDock, 28, 28);
		draw_glyph(view, kGlyphChevronUp, fHandle.InsetByCopy(14, 6),
			with_alpha(Theme::kText, 200));

		for (const Cell& cell : fHomeCells)
			_RenderCell(view, cell, 0);
	} else {
		view->SetHighColor(with_alpha(Theme::kPanel, 232));
		view->FillRect(bounds);

		BRect clip = bounds;
		clip.top = kDrawerTop - 12;
		clip.bottom -= Theme::kNavBarHeight;
		BRegion region(clip);
		view->ConstrainClippingRegion(&region);
		for (const Cell& cell : fDrawerCells) {
			if (cell.frame.bottom - fScroll >= clip.top
				&& cell.frame.top - fScroll <= clip.bottom) {
				_RenderCell(view, cell, -fScroll);
			}
		}
		view->ConstrainClippingRegion(NULL);

		view->SetFont(be_bold_font);
		view->SetFontSize(26);
		view->SetHighColor(Theme::kText);
		view->DrawString(fSettingsOnly ? "Settings" : "Applications",
			BPoint(28, Theme::kStatusBarHeight + 58));

		// A grabber to pull the drawer closed.
		view->SetHighColor(with_alpha(Theme::kText, 120));
		view->FillRoundRect(BRect(bounds.Width() / 2 - 32,
			Theme::kStatusBarHeight + 12, bounds.Width() / 2 + 32,
			Theme::kStatusBarHeight + 17), 3, 3);
	}

	view->Sync();
	fBuffer->Unlock();
	Invalidate();
}


void
HomeView::_RenderCell(BView* view, const Cell& cell, float offset)
{
	BRect frame = cell.frame.OffsetByCopy(0, offset);
	float iconSize = Theme::kIconSize;
	BRect icon(0, 0, iconSize - 1, iconSize - 1);
	float iconTop = cell.label ? frame.top + 18
		: frame.top + (frame.Height() - iconSize) / 2;
	icon.OffsetTo(frame.left + floorf((frame.Width() - iconSize) / 2), iconTop);

	if (&cell == fPressedCell) {
		view->SetHighColor(with_alpha(Theme::kText, 50));
		view->FillRoundRect(icon.InsetByCopy(-12, -12), Theme::kCorner,
			Theme::kCorner);
	}

	if (cell.app->icon != NULL)
		view->DrawBitmap(cell.app->icon, icon);
	else {
		view->SetHighColor(Theme::kSurfaceRaised);
		view->FillRoundRect(icon, Theme::kCorner, Theme::kCorner);
	}

	if (!cell.label)
		return;

	BFont font(be_plain_font);
	font.SetSize(16);
	font.SetFlags(B_FORCE_ANTIALIASING);
	view->SetFont(&font);
	BString label = cell.app->name;
	font.TruncateString(&label, B_TRUNCATE_END, frame.Width() - 12);
	float width = font.StringWidth(label.String());
	BPoint where(frame.left + floorf((frame.Width() - width) / 2),
		icon.bottom + 30);
	view->SetHighColor(0, 0, 0, 160);
	view->DrawString(label.String(), where + BPoint(1, 1));
	view->SetHighColor(Theme::kText);
	view->DrawString(label.String(), where);
}


void
HomeView::_RenderClock(BView* view)
{
	time_t now = time(NULL);
	struct tm local;
	localtime_r(&now, &local);

	char text[64];
	strftime(text, sizeof(text), "%H:%M", &local);
	BFont font(be_plain_font);
	font.SetSize(92);
	font.SetFace(B_LIGHT_FACE);
	// Subpixel antialiasing fringes in colour over the wallpaper.
	font.SetFlags(B_FORCE_ANTIALIASING);
	view->SetFont(&font);
	float width = font.StringWidth(text);
	font_height height;
	font.GetHeight(&height);
	BPoint where((Bounds().Width() - width) / 2, kClockTop + height.ascent);
	view->SetHighColor(0, 0, 0, 90);
	view->DrawString(text, where + BPoint(2, 2));
	view->SetHighColor(Theme::kText);
	view->DrawString(text, where);

	strftime(text, sizeof(text), "%A, %e %B", &local);
	font.SetSize(22);
	font.SetFace(B_REGULAR_FACE);
	view->SetFont(&font);
	width = font.StringWidth(text);
	where = BPoint((Bounds().Width() - width) / 2,
		where.y + height.descent + 38);
	view->SetHighColor(0, 0, 0, 90);
	view->DrawString(text, where + BPoint(1, 1));
	view->SetHighColor(with_alpha(Theme::kText, 230));
	view->DrawString(text, where);
}


void
HomeView::_SetDrawer(bool open, bool settings)
{
	fDrawerOpen = open;
	if (settings != fSettingsOnly) {
		fSettingsOnly = settings;
		_Layout();
	}
	fScroll = 0;
	fPressedCell = NULL;
	_Render();
}


float
HomeView::_MaxScroll() const
{
	if (fDrawerCells.empty())
		return 0;
	float contentBottom = fDrawerCells.back().frame.bottom + 24;
	float visibleBottom = Bounds().bottom - Theme::kNavBarHeight;
	return std::max(0.0f, contentBottom - visibleBottom);
}


const HomeView::Cell*
HomeView::_CellAt(BPoint where) const
{
	if (fDrawerOpen) {
		if (where.y < kDrawerTop - 12
			|| where.y > Bounds().bottom - Theme::kNavBarHeight) {
			return NULL;
		}
		for (const Cell& cell : fDrawerCells) {
			if (cell.frame.OffsetByCopy(0, -fScroll).Contains(where))
				return &cell;
		}
		return NULL;
	}

	for (const Cell& cell : fHomeCells) {
		if (cell.frame.Contains(where))
			return &cell;
	}
	return NULL;
}


// #pragma mark - HomeWindow


HomeWindow::HomeWindow(BRect frame, AppList* apps)
	:
	BWindow(frame, "Home", B_NO_BORDER_WINDOW_LOOK, kDesktopWindowFeel,
		B_NOT_MOVABLE | B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
			| B_NOT_CLOSABLE | B_ASYNCHRONOUS_CONTROLS,
		B_ALL_WORKSPACES)
{
	fView = new HomeView(Bounds(), apps);
	AddChild(fView);
}
