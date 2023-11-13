// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#ifndef _HELPER_HPP
#define _HELPER_HPP

#include <stdint.h>
#include <string>
#include <vector>
#include <wchar.h>

#include <windows.h>

bool FileExists(const std::string& szPath);
bool IsDirectory(const std::string& path);
bool CreatePrivateDirectory(const std::string& path);
bool EnsureCreatedDirectory(const std::string& path);
std::string ExpandUserFromStringA(const char* path, size_t path_len);
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

#endif // _HELPER_HPP
