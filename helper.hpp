// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#ifndef _HELPER_HPP
#define _HELPER_HPP

#include <vector>
#include <windows.h>
#include <stdint.h>

bool FileExists(LPCSTR szPath);
bool OpenKey(HKEY *hkey, bool isWriteOnly, bool isGlobal);
bool ReadKey(HKEY hkey, bool isGlobal, bool isData, std::vector<uint8_t>* output);
bool WriteKey(HKEY hkey, bool isGlobal, bool isData, const std::vector<uint8_t>& output);
bool CloseKey(HKEY hkey);

#endif // _HELPER_HPP