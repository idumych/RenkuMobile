/*
 * SystemStatus: what the status bar and quick settings show about the device.
 */
#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <String.h>


enum NetworkState {
	kNetworkOffline,
	kNetworkWired,
	kNetworkWireless
};


struct SystemStatus {
	NetworkState	network;
	BString			networkName;	// interface, or the Wi-Fi network
	bool			hasBattery;
	float			batteryLevel;	// 0..1, when hasBattery

	void			Refresh();
};


#endif	// SYSTEM_STATUS_H
