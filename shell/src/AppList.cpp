/*
 * AppList: see AppList.h.
 */
#include "AppList.h"

#include "Theme.h"

#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Directory.h>
#include <File.h>
#include <LocaleRoster.h>
#include <NodeInfo.h>
#include <Roster.h>

#include <algorithm>
#include <string.h>


// shortcut: fixed favourites, add a settings file once users can edit the home screen.
static const char* kDefaultDock[] = {
	"application/x-vnd.Haiku-WebPositive",
	"application/x-vnd.Be-MAIL",
	"application/x-vnd.Haiku-Terminal",
	"application/x-vnd.Haiku-MediaPlayer",
	NULL
};

static const char* kDefaultGrid[] = {
	"application/x-vnd.Haiku-StyledEdit",
	"application/x-vnd.Haiku-ShowImage",
	"application/x-vnd.Be-PEPL",
	"application/x-vnd.Haiku-DeskCalc",
	"application/x-vnd.Haiku-HaikuDepot",
	"application/x-vnd.Haiku-SoundRecorder",
	"application/x-vnd.Haiku-ActivityMonitor",
	"application/x-vnd.Haiku-CharacterMap",
	"application/x-vnd.haiku-icon_o_matic",
	"application/x-vnd.Haiku-Screen",
	"application/x-vnd.Haiku-Network",
	"application/x-vnd.Haiku-Appearance",
	NULL
};

// Apps that make no sense on the phone (the shell replaces them) or that
// fight it for the desktop.
static const char* kHidden[] = {
	"application/x-vnd.Be-TRAK",
	"application/x-vnd.Be-TSKB",
	"application/x-vnd.Haiku-Installer",
	"application/x-vnd.Haiku-LaunchBox",
	"application/x-vnd.Haiku-BootManager",
	"application/x-vnd.Haiku-Workspaces",
	SHELL_SIGNATURE,
	NULL
};


static bool
is_hidden(const char* signature)
{
	for (int32 i = 0; kHidden[i] != NULL; i++) {
		if (strcasecmp(kHidden[i], signature) == 0)
			return true;
	}
	return false;
}


AppList::AppList()
{
}


AppList::~AppList()
{
	_Clear();
}


void
AppList::Scan()
{
	_Clear();

	_ScanDirectory("/boot/system/apps", false);
	_ScanDirectory("/boot/system/preferences", true);
	_ScanDirectory("/boot/system/non-packaged/apps", false);
	_ScanDirectory("/boot/home/config/apps", false);
	_ScanDirectory("/boot/home/config/non-packaged/apps", false);

	std::sort(fApps.begin(), fApps.end(), [](AppEntry* a, AppEntry* b) {
		return a->name.ICompare(b->name) < 0;
	});

	_LoadFavorites();
}


AppEntry*
AppList::FindBySignature(const char* signature) const
{
	for (AppEntry* app : fApps) {
		if (app->signature.ICompare(signature) == 0)
			return app;
	}
	return NULL;
}


void
AppList::_ScanDirectory(const char* path, bool preference)
{
	BDirectory directory(path);
	if (directory.InitCheck() != B_OK)
		return;

	BEntry entry;
	while (directory.GetNextEntry(&entry, true) == B_OK) {
		if (entry.IsDirectory())
			continue;

		BFile file(&entry, B_READ_ONLY);
		BAppFileInfo info(&file);
		char signature[B_MIME_TYPE_LENGTH];
		if (info.InitCheck() != B_OK || info.GetSignature(signature) != B_OK)
			continue;

		uint32 flags = 0;
		if (info.GetAppFlags(&flags) == B_OK && (flags & B_BACKGROUND_APP) != 0)
			continue;
		if (is_hidden(signature) || FindBySignature(signature) != NULL)
			continue;

		AppEntry* app = new AppEntry;
		entry.GetRef(&app->ref);
		app->signature = signature;
		app->preference = preference;

		if (BLocaleRoster::Default()->GetLocalizedFileName(app->name, app->ref,
				false) != B_OK) {
			app->name = app->ref.name;
		}

		app->icon = new BBitmap(BRect(0, 0, Theme::kIconSize - 1,
			Theme::kIconSize - 1), B_RGBA32);
		if (BNodeInfo::GetTrackerIcon(&app->ref, app->icon,
				(icon_size)Theme::kIconSize) != B_OK) {
			delete app->icon;
			app->icon = NULL;
		}

		fApps.push_back(app);
	}
}


void
AppList::_LoadFavorites()
{
	for (int32 i = 0; kDefaultDock[i] != NULL; i++) {
		if (AppEntry* app = FindBySignature(kDefaultDock[i]))
			fDock.push_back(app);
	}
	for (int32 i = 0; kDefaultGrid[i] != NULL; i++) {
		if (AppEntry* app = FindBySignature(kDefaultGrid[i]))
			fFavorites.push_back(app);
	}
}


void
AppList::_Clear()
{
	for (AppEntry* app : fApps) {
		delete app->icon;
		delete app;
	}
	fApps.clear();
	fFavorites.clear();
	fDock.clear();
}
