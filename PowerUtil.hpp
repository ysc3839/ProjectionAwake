#pragma once

bool g_hasLid = false;
bool g_hasBattery = false;

DISPLAYCONFIG_TOPOLOGY_ID GetDisplayConfigTopology()
{
	UINT32 pathCount, modeInfoCount;
	if (SUCCEEDED_WIN32(LOG_IF_WIN32_ERROR(GetDisplayConfigBufferSizes(QDC_DATABASE_CURRENT, &pathCount, &modeInfoCount))))
	{
		auto path = std::make_unique<DISPLAYCONFIG_PATH_INFO[]>(pathCount);
		auto modeInfo = std::make_unique<DISPLAYCONFIG_MODE_INFO[]>(modeInfoCount);
		DISPLAYCONFIG_TOPOLOGY_ID currTopologyId;
		if (SUCCEEDED_WIN32(LOG_IF_WIN32_ERROR(QueryDisplayConfig(QDC_DATABASE_CURRENT, &pathCount, path.get(), &modeInfoCount, modeInfo.get(), &currTopologyId))))
		{
			return currTopologyId;
		}
	}
	return DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32;
}

constexpr bool IsExternalTopology(DISPLAYCONFIG_TOPOLOGY_ID topology)
{
	return topology == DISPLAYCONFIG_TOPOLOGY_CLONE ||
		topology == DISPLAYCONFIG_TOPOLOGY_EXTEND ||
		topology == DISPLAYCONFIG_TOPOLOGY_EXTERNAL;
}

bool IsPowerSettingEnabled(const wchar_t* valName, bool initVal)
{
	wil::unique_hkey hKey;
	if (SUCCEEDED_WIN32(RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\ControlPanel\NameSpace\{025A5937-A6BE-4686-A844-36FE4BEC8B6D}\HardwareOverride)",
		0, KEY_READ, &hKey)))
	{
		DWORD data, type, size = sizeof(data);
		if (SUCCEEDED_WIN32(RegQueryValueExW(hKey.get(), valName, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size)))
			initVal = (data != 0);
	}
	return initVal;
}

HRESULT InitPowerCapabilities()
{
	SYSTEM_POWER_CAPABILITIES sysPowerCap;
	RETURN_LAST_ERROR_IF(!GetPwrCapabilities(&sysPowerCap));
	g_hasLid = IsPowerSettingEnabled(L"LidSwitch", sysPowerCap.LidPresent);
	g_hasBattery = IsPowerSettingEnabled(L"BatteryEnabled", sysPowerCap.SystemBatteriesPresent || sysPowerCap.UpsPresent);
	return S_OK;
}

std::wstring GetPowerFriendlyName(const GUID* schemeGuid, const GUID* subGroupGuid, const GUID* powerSettingGuid)
{
	std::wstring buf(64, L'\0');
	while (true)
	{
		DWORD size = static_cast<DWORD>(buf.size() * sizeof(std::wstring::value_type));
		auto err = PowerReadFriendlyName(nullptr, schemeGuid, subGroupGuid, powerSettingGuid, reinterpret_cast<PUCHAR>(buf.data()), &size);
		if (err == ERROR_SUCCESS)
			buf.resize((size / sizeof(std::wstring::value_type)) - 1);
		else if (err == ERROR_MORE_DATA)
		{
			buf.resize(size / sizeof(std::wstring::value_type));
			continue;
		}
		else
		{
			buf.clear();
			LOG_WIN32(err);
		}
		break;
	}
	return buf;
}

HRESULT GetActivePowerScheme(GUID& scheme)
{
	wil::unique_any<LPGUID, decltype(&::LocalFree), ::LocalFree> activePolicyGuid;
	RETURN_IF_WIN32_ERROR(PowerGetActiveScheme(nullptr, activePolicyGuid.put()));
	scheme = *activePolicyGuid.get();
	return S_OK;
}

void SetPowerConfigValues(LPCGUID scheme, PowerCfgValue val[])
{
	for (size_t i = 0; i < POWER_CFG_COUNT; ++i)
	{
		LOG_IF_WIN32_ERROR(PowerWriteACValueIndex(nullptr, scheme, POWER_CFG[i].first, POWER_CFG[i].second, val[i].first));
		if (g_hasBattery)
			LOG_IF_WIN32_ERROR(PowerWriteDCValueIndex(nullptr, scheme, POWER_CFG[i].first, POWER_CFG[i].second, val[i].second));
	}
}

void SaveOrRestorePowerConfigs(LPCGUID scheme, bool save)
{
	static PowerCfgValue configValues[POWER_CFG_COUNT];
	if (save)
	{
		for (size_t i = 0; i < POWER_CFG_COUNT; ++i)
		{
			LOG_IF_WIN32_ERROR(PowerReadACValueIndex(nullptr, scheme, POWER_CFG[i].first, POWER_CFG[i].second, &configValues[i].first));
			if (g_hasBattery)
				LOG_IF_WIN32_ERROR(PowerReadDCValueIndex(nullptr, scheme, POWER_CFG[i].first, POWER_CFG[i].second, &configValues[i].second));
		}
	}
	else
	{
		SetPowerConfigValues(scheme, configValues);
	}
}
