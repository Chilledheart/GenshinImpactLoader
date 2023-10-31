// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#include "helper.hpp"

#if defined(_MSC_VER)
#pragma comment(lib, "advapi32")
#endif

static const wchar_t* kGenshinImpactCnPathKey = L"Software\\miHoYo\\原神";
static const wchar_t* kGenshinImpactCnSdkKey = L"MIHOYOSDK_ADL_PROD_CN_h3123967166";
static const wchar_t* kGenshinImpactGlobalPathKey = L"Software\\miHoYo\\Genshin Impact";
static const wchar_t* kGenshinImpactGlobalSdkKey = L"MIHOYOSDK_ADL_PROD_OVERSEA_h1158948810";
static const wchar_t* kGenshinImpactDataKey = L"GENERAL_DATA_h2389025596";

static constexpr size_t kRegReadMaximumSize = 1024 * 1024;

static_assert(sizeof(BYTE) == sizeof(uint8_t));
static_assert(alignof(BYTE) == alignof(uint8_t));

bool FileExists(LPCSTR szPath) {
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool OpenKey(HKEY *hkey, bool isWriteOnly, bool isGlobal) {
    DWORD disposition;
    const wchar_t* subkey = isGlobal ? kGenshinImpactGlobalPathKey : kGenshinImpactCnPathKey;
    REGSAM samDesired =
        KEY_WOW64_64KEY | (isWriteOnly ? KEY_SET_VALUE : KEY_QUERY_VALUE);

    // Creates the specified registry key. If the key already exists, the
    // function opens it. Note that key names are not case sensitive.
    if (::RegCreateKeyExW(
            HKEY_CURRENT_USER /*hKey*/, subkey /* lpSubKey */, 0 /* Reserved */,
            nullptr /*lpClass*/, REG_OPTION_NON_VOLATILE /* dwOptions */,
            samDesired /* samDesired */, nullptr /*lpSecurityAttributes*/,
            hkey /* phkResult */,
            &disposition /* lpdwDisposition*/) == ERROR_SUCCESS) {
        if (disposition == REG_CREATED_NEW_KEY) {
        } else if (disposition == REG_OPENED_EXISTING_KEY) {
        }
        return true;
    }
    return false;
}

bool ReadKey(HKEY hkey, bool isGlobal, bool isData, std::vector<uint8_t>* output) {
    const wchar_t* valueName = isData ? kGenshinImpactDataKey : (isGlobal ? kGenshinImpactGlobalSdkKey : kGenshinImpactCnSdkKey);
    DWORD BufferSize;
    DWORD type;

    // If lpData is nullptr, and lpcbData is non-nullptr, the function returns
    // ERROR_SUCCESS and stores the size of the data, in bytes, in the variable
    // pointed to by lpcbData. This enables an application to determine the best
    // way to allocate a buffer for the value's data.
    if (::RegQueryValueExW(hkey /* HKEY */, valueName /* lpValueName */,
                           nullptr /* lpReserved */, &type /* lpType */,
                           nullptr /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
        return false;
    }

    if (type != REG_BINARY || BufferSize > kRegReadMaximumSize) {
        return false;
    }

    output->resize(BufferSize);
    if (::RegQueryValueExW(hkey /* HKEY */,
                           valueName /* lpValueName */,
                           nullptr /* lpReserved */,
                           &type /* lpType */,
                           output->data() /* lpData */,
                           &BufferSize /* lpcbData */) != ERROR_SUCCESS) {
        return false;
    }
    output->reserve(BufferSize);
    return true;
}

bool WriteKey(HKEY hkey, bool isGlobal, bool isData, const std::vector<uint8_t>& output) {
    const wchar_t* valueName = isData ? kGenshinImpactDataKey : (isGlobal ? kGenshinImpactGlobalSdkKey : kGenshinImpactCnSdkKey);
    if (::RegSetValueExW(hkey /* hKey*/,
                         valueName /*lpValueName*/,
                         0 /*Reserved*/,
                         REG_BINARY /*dwType*/,
                         output.data() /*lpData*/,
                         static_cast<DWORD>(output.size()) /*cbData*/) == ERROR_SUCCESS) {
        return true;
    }
    return false;
}

bool CloseKey(HKEY hkey) {
    return ::RegCloseKey(hkey);
}

