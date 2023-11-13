// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#include "helper.hpp"

#include <cassert>
#include <stddef.h>

#include <bcrypt.h>
#include <shlobj.h>

#if defined(_MSC_VER)
#pragma comment(lib, "advapi32")
#pragma comment(lib, "bcrypt")
#pragma comment(lib, "shell32")
#endif

static constexpr size_t kRegReadMaximumSize = 1024 * 1024;

static_assert(sizeof(BYTE) == sizeof(uint8_t));
static_assert(alignof(BYTE) == alignof(uint8_t));

bool FileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
           !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool IsDirectory(const std::wstring& path) {
    if (path == L"." || path == L"..") {
        return true;
    }

    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
             (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool CreatePrivateDirectory(const std::wstring& path) {
    return ::CreateDirectoryW(path.c_str(), nullptr) == TRUE;
}

bool EnsureCreatedDirectory(const std::wstring& path) {
    return IsDirectory(path) || CreatePrivateDirectory(path);
}

std::wstring ExpandUserFromString(const std::wstring& path) {
    size_t path_len = path.size();
    // the return value is the REQUIRED number of TCHARs,
    // including the terminating NULL character.
    DWORD required_size = ::ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);

    // The size of the lpSrc and lpDst buffers is limited to 32K.
    /* if failure or too many bytes required, documented in
     * ExpandEnvironmentStringsW */
    if (required_size == 0 || required_size >= 32 * 1024) {
        return std::wstring(path, path_len ? path_len - 1 : path_len);
    }

    std::wstring expanded_path;
    expanded_path.resize(required_size);
    ::ExpandEnvironmentStringsW(path.c_str(), &expanded_path[0], required_size);
    while (!expanded_path.empty() && expanded_path[expanded_path.size() - 1] == L'\0') {
        expanded_path.resize(expanded_path.size() - 1);
    }

    return expanded_path;
}

std::wstring GetLocalAppPath() {
    wchar_t system_buffer[32 * 1024];
    system_buffer[0] = 0;
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                                SHGFP_TYPE_CURRENT, system_buffer))) {
        abort();
        return std::wstring();
    }
    return std::wstring(system_buffer);
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

void RandBytes(PUCHAR out, size_t requested) {
    while (requested > 0) {
        ULONG output_bytes_this_pass = ULONG_MAX;
        if (requested < output_bytes_this_pass) {
            output_bytes_this_pass = (ULONG)requested;
        }
        if (!BCRYPT_SUCCESS(BCryptGenRandom(
                /*hAlgorithm=*/NULL, out, output_bytes_this_pass,
                BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
            abort();
        }
        requested -= output_bytes_this_pass;
        out += output_bytes_this_pass;
    }
}

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToUTF8(const std::wstring& wide) {
    return SysWideToMultiByte(wide, CP_UTF8);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysUTF8ToWide(std::string_view utf8) {
    return SysMultiByteToWide(utf8, CP_UTF8);
}

// Do not assert in this function since it is used by the asssertion code!
std::wstring SysMultiByteToWide(std::string_view mb, uint32_t code_page) {
    int mb_length = static_cast<int>(mb.length());
    // Note that, if cbMultiByte is 0, the function fails.
    if (mb_length == 0)
        return std::wstring();

    // Compute the length of the buffer.
    int charcount = MultiByteToWideChar(code_page /* CodePage */,
                                        0 /* dwFlags */,
                                        mb.data() /* lpMultiByteStr */,
                                        mb_length /* cbMultiByte */,
                                        nullptr /* lpWideCharStr */,
                                        0 /* cchWideChar */);
    // The function returns 0 if it does not succeed.
    if (charcount == 0)
        return std::wstring();

    // If the function succeeds and cchWideChar is 0,
    // the return value is the required size, in characters,
    std::wstring wide;
    wide.resize(charcount);
    MultiByteToWideChar(code_page /* CodePage */, 0 /* dwFlags */,
                        mb.data() /* lpMultiByteStr */,
                        mb_length /* cbMultiByte */,
                        &wide[0] /* lpWideCharStr */,
                        charcount /* cchWideChar */);

    return wide;
}

// Do not assert in this function since it is used by the asssertion code!
std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page) {
    int wide_length = static_cast<int>(wide.length());
    // If cchWideChar is set to 0, the function fails.
    if (wide_length == 0)
        return std::string();

    // Compute the length of the buffer we'll need.
    int charcount = WideCharToMultiByte(code_page /* CodePage */,
                                        0 /* dwFlags */,
                                        wide.data() /* lpWideCharStr */,
                                        wide_length /* cchWideChar */,
                                        nullptr /* lpMultiByteStr */,
                                        0 /* cbMultiByte */,
                                        nullptr /* lpDefaultChar */,
                                        nullptr /* lpUsedDefaultChar */);
    // The function returns 0 if it does not succeed.
    if (charcount == 0)
        return std::string();
    // If the function succeeds and cbMultiByte is 0, the return value is
    // the required size, in bytes, for the buffer indicated by lpMultiByteStr.
    std::string mb;
    mb.resize(charcount);

    WideCharToMultiByte(code_page /* CodePage */, 0 /* dwFlags */,
                        wide.data() /* lpWideCharStr */, wide_length /* cchWideChar */,
                        &mb[0] /* lpMultiByteStr */, charcount /* cbMultiByte */,
                        nullptr /* lpDefaultChar */, nullptr /* lpUsedDefaultChar */);

    return mb;
}
