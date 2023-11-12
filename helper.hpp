// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#ifndef _HELPER_HPP
#define _HELPER_HPP

#include <vector>
#include <windows.h>
#include <stdint.h>

bool FileExists(LPCSTR szPath);
bool OpenKey(HKEY *hkey, bool isWriteOnly, const wchar_t* subkey);
bool ReadKey(HKEY hkey, const wchar_t *valueName, std::vector<uint8_t>* output);
bool WriteKey(HKEY hkey, const wchar_t *valueName, const std::vector<uint8_t>& output);
bool CloseKey(HKEY hkey);
void RandBytes(void* output, size_t output_length);
template<typename T>
T Rand() {
  T t;
  RandBytes(&t, sizeof(t));
  return t;
}

#endif // _HELPER_HPP
