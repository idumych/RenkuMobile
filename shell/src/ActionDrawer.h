/*
 * ActionDrawer: pulled down from the status bar. The date and time and a grid
 * of quick-setting tiles, as in Plasma Mobile's action drawer.
 */
#ifndef ACTION_DRAWER_H
#define ACTION_DRAWER_H

#include "Glyphs.h"
#include "Overlay.h"
#include "SystemStatus.h"

#include <vector>


class ActionDrawerView : public OverlayView {
public:
								ActionDrawerView(BRect frame);

	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseUp(BPoint where);

			void				Open();
			void				Close();
			void				SetKeyboardShown(bool shown);

protected:
	virtual	void				RenderContent(BView* view);

private:
	enum Tile { kTileNetwork, kTileKeyboard, kTileScreenshot, kTileSettings,
		kTileCount };

			BRect				_TileFrame(int32 tile) const;
			int32				_TileAt(BPoint where) const;
			float				_PanelHeight() const;

			SystemStatus		fStatus;
			bool				fKeyboardShown;
			bool				fTracking;
			BPoint				fDownPoint;
			int32				fPressed;
};


#endif	// ACTION_DRAWER_H
