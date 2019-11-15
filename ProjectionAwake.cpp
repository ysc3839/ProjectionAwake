#include "pch.h"
#include "ProjectionAwake.h"

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	g_hInst = hInstance;
	InitPowerCapabilities();
	std::get<0>(g_userConfigValues[0]) = !g_hasLid;

	WNDCLASSEXW wcex = {
		.cbSize = sizeof(wcex),
		.lpfnWndProc = WndProc,
		.hInstance = hInstance,
		.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_PROJECTIONAWAKE)),
		.hCursor = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = L"ProjectionAwake",
		.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_PROJECTIONAWAKE))
	};

	RegisterClassExW(&wcex);

	g_hWnd = CreateWindowW(L"ProjectionAwake", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
		return EXIT_FAILURE;

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return static_cast<int>(msg.wParam);
}

HRESULT ShowContextMenu(HWND hWnd, int x, int y)
{
	wil::unique_hmenu hMenu(LoadMenuW(g_hInst, MAKEINTRESOURCEW(IDC_PROJECTIONAWAKE)));
	RETURN_LAST_ERROR_IF_NULL(hMenu);

	HMENU hSubMenu = GetSubMenu(hMenu.get(), 0);
	RETURN_HR_IF_NULL(E_INVALIDARG, hSubMenu);

	RETURN_IF_WIN32_BOOL_FALSE(SetForegroundWindow(hWnd));

	UINT flags = TPM_RIGHTBUTTON;
	if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
		flags |= TPM_RIGHTALIGN;
	else
		flags |= TPM_LEFTALIGN;

	RETURN_IF_WIN32_BOOL_FALSE(TrackPopupMenuEx(hSubMenu, flags, x, y, hWnd, nullptr));
	return S_OK;
}

void ShowBalloon(HWND hWnd, const wchar_t* info)
{
	NOTIFYICONDATA nid = {
		.cbSize = sizeof(nid),
		.hWnd = hWnd,
		.uFlags = NIF_INFO,
		.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME
	};
	wcscpy_s(nid.szInfoTitle, _(L"ProjectionAwake"));
	wcscpy_s(nid.szInfo, info);
	LOG_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_MODIFY, &nid));
}

void OnTopologyChange(HWND hWnd, DISPLAYCONFIG_TOPOLOGY_ID dispTopology, DISPLAYCONFIG_TOPOLOGY_ID newTopology)
{
	if (dispTopology == DISPLAYCONFIG_TOPOLOGY_INTERNAL && IsExternalTopology(newTopology))
	{
		SaveOrRestorePowerConfigs(&g_powerScheme, true);
		SetPowerConfigValues(&g_powerScheme, g_userConfigValues);
		PowerSetActiveScheme(nullptr, &g_powerScheme);
		ShowBalloon(hWnd, _(L"External display connected."));
	}
	else if (newTopology == DISPLAYCONFIG_TOPOLOGY_INTERNAL && IsExternalTopology(dispTopology))
	{
		SaveOrRestorePowerConfigs(&g_powerScheme, false);
		PowerSetActiveScheme(nullptr, &g_powerScheme);
		ShowBalloon(hWnd, _(L"External display disconnected."));
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static UINT WM_TASKBAR_CREATED;
	static NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
		.uCallbackMessage = WM_NOTIFYICON,
		.uVersion = NOTIFYICON_VERSION_4
	};
	static wil::unique_hpowernotify hPowerNotify;
	static DISPLAYCONFIG_TOPOLOGY_ID dispTopology = DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32;
	switch (message)
	{
	case WM_CREATE:
	{
		nid.hWnd = hWnd;
		nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_PROJECTIONAWAKE));
		wcscpy_s(nid.szTip, _(L"ProjectionAwake"));

		FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_ADD, &nid));
		FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_SETVERSION, &nid));

		WM_TASKBAR_CREATED = RegisterWindowMessageW(L"TaskbarCreated");
		LOG_LAST_ERROR_IF(WM_TASKBAR_CREATED == 0);

		hPowerNotify.reset(RegisterPowerSettingNotification(hWnd, &GUID_POWERSCHEME_PERSONALITY, DEVICE_NOTIFY_WINDOW_HANDLE));
		LOG_LAST_ERROR_IF_NULL(hPowerNotify);

		LOG_IF_FAILED(GetActivePowerScheme(g_powerScheme));

		dispTopology = GetDisplayConfigTopology();
		if (dispTopology != DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32)
			OnTopologyChange(hWnd, DISPLAYCONFIG_TOPOLOGY_INTERNAL, dispTopology);
	}
	break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_SETTINGS:
		{
			static bool dialogOpened = false;
			static HWND hDlg;
			if (dialogOpened)
			{
				SetForegroundWindow(hDlg);
			}
			else
			{
				dialogOpened = true;
				DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS), nullptr, About, reinterpret_cast<LPARAM>(&hDlg));
				dialogOpened = false;
			}
		}
		break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DESTROY:
		hPowerNotify.reset(nullptr);
		SaveOrRestorePowerConfigs(&g_powerScheme, false);
		Shell_NotifyIconW(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;
	case WM_DISPLAYCHANGE:
	{
		auto newTopology = GetDisplayConfigTopology();
		if (newTopology != DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32 &&
			dispTopology != DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32)
		{
			OnTopologyChange(hWnd, dispTopology, newTopology);
			dispTopology = newTopology;
		}
	}
	break;
	case WM_POWERBROADCAST:
		if (wParam == PBT_POWERSETTINGCHANGE)
		{
			auto pbs = reinterpret_cast<PPOWERBROADCAST_SETTING>(lParam);
			if (pbs->PowerSetting == GUID_POWERSCHEME_PERSONALITY && pbs->DataLength == sizeof(GUID))
			{
				auto newPowerScheme = reinterpret_cast<LPCGUID>(&pbs->Data);
				if (g_powerScheme != *newPowerScheme)
				{
					ShowBalloon(hWnd, (_(L"Power scheme changed. New scheme name: ") + GetPowerConfigText<FriendlyName>(nullptr, nullptr, newPowerScheme)).c_str());
					if (IsExternalTopology(dispTopology))
					{
						SaveOrRestorePowerConfigs(&g_powerScheme, false);
						SaveOrRestorePowerConfigs(newPowerScheme, true);
						SetPowerConfigValues(newPowerScheme, g_userConfigValues);
						PowerSetActiveScheme(nullptr, newPowerScheme);
					}
					g_powerScheme = *newPowerScheme;
				}
			}
		}
		break;
	case WM_NOTIFYICON:
		switch (LOWORD(lParam))
		{
		case NIN_SELECT:
		case NIN_KEYSELECT:
			PostMessageW(hWnd, WM_COMMAND, IDM_SETTINGS, 0);
			break;
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd, LOWORD(wParam), HIWORD(wParam));
			break;
		}
		break;
	default:
		if (WM_TASKBAR_CREATED && message == WM_TASKBAR_CREATED)
		{
			if (!Shell_NotifyIconW(NIM_MODIFY, &nid))
			{
				FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_ADD, &nid));
				FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_SETVERSION, &nid));
			}
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		*reinterpret_cast<HWND*>(lParam) = hDlg;
		auto text = GetPowerConfigText<FriendlyName>(POWER_CFG[0].first, POWER_CFG[0].second, &g_powerScheme);
		text += L':';
		SetDlgItemTextW(hDlg, IDC_LIDNAME, text.c_str());

		text = GetPowerConfigText<FriendlyName>(POWER_CFG[1].first, POWER_CFG[1].second, &g_powerScheme);
		text += L" (" + GetPowerConfigText<ValueUnitsSpecifier>(POWER_CFG[1].first, POWER_CFG[1].second) + L"):";
		SetDlgItemTextW(hDlg, IDC_DISPNAME, text.c_str());

		text = GetPowerConfigText<FriendlyName>(POWER_CFG[2].first, POWER_CFG[2].second, &g_powerScheme);
		text += L" (" + GetPowerConfigText<ValueUnitsSpecifier>(POWER_CFG[2].first, POWER_CFG[2].second) + L"):";
		SetDlgItemTextW(hDlg, IDC_SLEEPNAME, text.c_str());
		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
