/*
 * Compositor: Renku's composited desktop, where the system has it. Renku
 * builds without it (the PinePhone's, for one) have neither the call nor the
 * window flag, so both are looked up rather than assumed.
 */
#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <InterfaceDefs.h>
#include <SupportDefs.h>


extern "C" bool is_composited_desktop() __attribute__((weak));

// B_TRANSPARENT_WINDOW on a composited desktop
static const uint32 kTransparentWindow = 0x80000000;


// app_server only answers this when it starts, so ask once.
static inline bool
renku_composited()
{
	static const bool sComposited = is_composited_desktop != NULL
		&& is_composited_desktop();
	return sComposited;
}


#endif	// COMPOSITOR_H
