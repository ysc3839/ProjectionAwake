#pragma once

#include "resource.h"

HINSTANCE g_hInst;
GUID g_powerScheme;
DISPLAYCONFIG_TOPOLOGY_ID g_dispTopology = DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32;
bool g_showNotify = true;
HWND g_hWnd;

constexpr UINT WM_NOTIFYICON = WM_APP + 1;

constexpr std::pair<LPCGUID, LPCGUID> POWER_CFG[] = {
	{ &GUID_SYSTEM_BUTTON_SUBGROUP, &GUID_LIDCLOSE_ACTION },
	{ &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT },
	{ &GUID_SLEEP_SUBGROUP, &GUID_STANDBY_TIMEOUT }
};
constexpr auto POWER_CFG_COUNT = std::size(POWER_CFG);

using PowerCfgValue = std::tuple<bool, DWORD, DWORD>; // <disabled, AC, DC>

PowerCfgValue g_userConfigValues[POWER_CFG_COUNT] = {
	{ false, 0, 0 },
	{ false, 1800, 1800 },
	{ false, 3600, 3600 }
};

#include "I18n.hpp"
#include "PowerUtil.hpp"
