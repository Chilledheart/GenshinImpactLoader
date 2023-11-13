// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#ifndef _HELPER_HPP
#define _HELPER_HPP

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>
#include <wchar.h>

#include <windows.h>

bool FileExists(const std::wstring& szPath);
bool IsDirectory(const std::wstring& path);
bool CreatePrivateDirectory(const std::wstring& path);
bool EnsureCreatedDirectory(const std::wstring& path);
std::wstring ExpandUserFromString(const std::wstring& path);

// Usually C:\Windows\Fonts.
std::wstring GetWindowsFontsPath();
// Local Application Data directory under the user profile.
// Usually "C:\Users\<user>\AppData\Local".
std::wstring GetLocalAppPath();

bool OpenKey(HKEY *hkey, bool isWriteOnly, const wchar_t* subkey);
bool ReadKey(HKEY hkey, const wchar_t *valueName, std::vector<uint8_t>* output);
bool WriteKey(HKEY hkey, const wchar_t *valueName, const std::vector<uint8_t>& output);
bool CloseKey(HKEY hkey);
void RandBytes(PUCHAR output, size_t output_length);
template<typename T>
T Rand() {
  T t;
  RandBytes(reinterpret_cast<PUCHAR>(&t), sizeof(t));
  return t;
}

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
std::string SysWideToUTF8(const std::wstring& wide);
std::wstring SysUTF8ToWide(std::string_view utf8);

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
std::wstring SysMultiByteToWide(std::string_view mb, uint32_t code_page);

std::string SysWideToMultiByte(const std::wstring& wide, uint32_t code_page);

#endif // _HELPER_HPP
