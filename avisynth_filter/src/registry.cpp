// License: https://github.com/CrendKing/avisynth_filter/blob/master/LICENSE

#include "pch.h"
#include "registry.h"
#include "constants.h"


namespace AvsFilter {

Registry::Registry()
    : _registryKey(nullptr) {
}

Registry::~Registry() {
    if (_registryKey) {
        RegCloseKey(_registryKey);
    }
}

auto Registry::Initialize() -> bool {
    return RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY_NAME, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &_registryKey, nullptr) == ERROR_SUCCESS;
}

Registry::operator bool() const {
    return _registryKey != nullptr;
}

auto Registry::ReadString(const WCHAR *valueName) const -> std::wstring {
    std::wstring ret;

    if (_registryKey) {
        std::array<WCHAR, MAX_PATH> buffer;
        DWORD bufferSize = static_cast<DWORD>(buffer.size());

        const LSTATUS registryStatus = RegGetValueW(_registryKey, nullptr, valueName, RRF_RT_REG_SZ, nullptr, buffer.data(), &bufferSize);
        if (registryStatus == ERROR_SUCCESS) {
            ret.assign(buffer.data(), bufferSize / sizeof(WCHAR) - 1);
        }
    }

    return ret;
}

auto Registry::ReadNumber(const WCHAR *valueName, int defaultValue) const -> DWORD {
    DWORD ret = defaultValue;

    if (_registryKey) {
        DWORD valueSize = sizeof(ret);
        RegGetValueW(_registryKey, nullptr, valueName, RRF_RT_REG_DWORD, nullptr, &ret, &valueSize);
    }

    return ret;
}

auto Registry::WriteString(const WCHAR *valueName, const std::wstring &valueString) const -> bool {
    return _registryKey && RegSetValueExW(_registryKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE *>(valueString.c_str()), static_cast<DWORD>(valueString.size() * 2 + 2)) == ERROR_SUCCESS;
}

auto Registry::WriteNumber(const WCHAR *valueName, DWORD valueNumber) const -> bool {
    return _registryKey && RegSetValueExW(_registryKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&valueNumber), sizeof(valueNumber)) == ERROR_SUCCESS;
}

}
