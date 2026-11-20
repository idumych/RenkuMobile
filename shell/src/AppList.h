/*
 * AppList: the apps the home screen and the app drawer show.
 */
#ifndef APP_LIST_H
#define APP_LIST_H

#include <Entry.h>
#include <String.h>

#include <vector>


class BBitmap;


struct AppEntry {
	BString		name;		// localized, as Deskbar shows it
	BString		signature;
	entry_ref	ref;
	BBitmap*	icon;		// kIconSize, owned by the list
	bool		preference;	// from /boot/system/preferences
};


class AppList {
public:
								AppList();
								~AppList();

			void				Scan();

			const std::vector<AppEntry*>& Apps() const { return fApps; }
			const std::vector<AppEntry*>& Favorites() const
									{ return fFavorites; }
			const std::vector<AppEntry*>& Dock() const { return fDock; }

			AppEntry*			FindBySignature(const char* signature) const;

private:
			void				_ScanDirectory(const char* path,
									bool preference);
			void				_LoadFavorites();
			void				_Clear();

			std::vector<AppEntry*> fApps;		// sorted by name
			std::vector<AppEntry*> fFavorites;	// home grid
			std::vector<AppEntry*> fDock;		// bottom row
};


#endif	// APP_LIST_H
