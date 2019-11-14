#pragma once

#include "resource.h"

HINSTANCE g_hInst;
GUID g_powerScheme;
HWND g_hWnd;

constexpr UINT WM_NOTIFYICON = WM_APP + 1;

constexpr std::pair<LPCGUID, LPCGUID> POWER_CFG[] = {
	{ &GUID_SYSTEM_BUTTON_SUBGROUP, &GUID_LIDCLOSE_ACTION },
	{ &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT },
	{ &GUID_SLEEP_SUBGROUP, &GUID_STANDBY_TIMEOUT }
};
constexpr auto POWER_CFG_COUNT = std::size(POWER_CFG);

using PowerCfgValue = std::pair<DWORD, DWORD>; // <AC, DC>

PowerCfgValue g_userConfigValues[POWER_CFG_COUNT] = {
	{ 0, 0 },
	{ 1800, 1800 },
	{ 3600, 3600 }
};

#include "I18n.hpp"
#include "PowerUtil.hpp"
