// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#include "helper.hpp"

#include <cassert>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>

#if defined(_MSC_VER)
#pragma comment(lib, "advapi32")
#endif

static constexpr size_t kRegReadMaximumSize = 1024 * 1024;

static_assert(sizeof(BYTE) == sizeof(uint8_t));
static_assert(alignof(BYTE) == alignof(uint8_t));

bool FileExists(LPCSTR szPath) {
    DWORD dwAttrib = GetFileAttributesA(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool OpenKey(HKEY *hkey, bool isWriteOnly, const wchar_t* subkey) {
    DWORD disposition;
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

bool ReadKey(HKEY hkey, const wchar_t *valueName, std::vector<uint8_t>* output) {
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

bool WriteKey(HKEY hkey, const wchar_t *valueName, const std::vector<uint8_t>& output) {
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

// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#ifdef COMPILER_MSVC
#include <NTSecAPI.h>
#else
#include <ntsecapi.h>
#endif
#undef SystemFunction036

// TODO: another alternative is BCryptGenRandom introduced in Windows Vista
// https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom

void RandBytes(void* output, size_t output_length) {
  char* output_ptr = static_cast<char*>(output);
  while (output_length > 0) {
    const ULONG output_bytes_this_pass = static_cast<ULONG>(std::min(
        output_length, static_cast<size_t>(std::numeric_limits<ULONG>::max())));
    const bool success =
        RtlGenRandom(output_ptr, output_bytes_this_pass) != FALSE;
    (void)success;
    assert(success && "RtlGenRandom failed");
    output_length -= output_bytes_this_pass;
    output_ptr += output_bytes_this_pass;
  }
}
