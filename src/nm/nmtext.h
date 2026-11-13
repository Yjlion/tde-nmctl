/*
 * tde-nmctl - NetworkManager front-end for the Trinity Desktop Environment
 *
 * Copyright (C) 2026 tde-nmctl developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef NMTEXT_H
#define NMTEXT_H

#include <tqstring.h>
#include <tqstringlist.h>

struct NMDeviceInfo;
struct NMActiveConnection;

/** Translated names, state descriptions and icon names. */
namespace NMText
{
	TQString deviceType(uint type);
	TQString deviceState(uint state);
	TQString stateReason(uint reason);
	TQString managerState(uint state);
	TQString connectivity(uint connectivity);
	TQString activeState(uint state);
	TQString vpnState(uint state);

	/** connection.type -> "Ethernet", "Wi-Fi", ... */
	TQString connectionType(const TQString &type);
	/** connection.type -> group shown in the editor list */
	TQString connectionGroup(const TQString &type);
	/** the connection types that can be created, in menu order */
	TQStringList creatableTypes();
	TQString connectionIcon(const TQString &type);
	TQString deviceIcon(uint type);

	/** tde-nmctl-signal-0 ... tde-nmctl-signal-100 */
	TQString signalIcon(uint strength);
	/** wired / wireless / vpn / offline icon for the tray */
	TQString trayIcon(const NMDeviceInfo *primary, uint managerState, bool vpn, uint strength);

	TQString bitrate(uint kbits);
	TQString bytes(TQ_UINT64 bytes);
	TQString frequency(uint mhz);
	TQString band(uint mhz);
}

#endif
