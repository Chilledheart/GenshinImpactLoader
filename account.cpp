// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#include "account.hpp"

#include <stdio.h>
#include <windows.h>

#include "helper.hpp"

static const char kGenshinImpactDbFileName[] = "GenshinImpactLoader.dat";

bool Account::Load() {
    HKEY hkey;
    if (!OpenKey(&hkey, false, is_global_))
        return false;

    if (!ReadKey(hkey, is_global_, false, &name_))
        goto failure;

    if (!ReadKey(hkey, is_global_, true, &data_))
        goto failure;

    CloseKey(hkey);
    return true;

failure:
    CloseKey(hkey);
    return false;
}

bool Account::Save() const {
    HKEY hkey;
    if (!OpenKey(&hkey, true, is_global_))
        return false;

    if (!WriteKey(hkey, is_global_, false, name_))
        goto failure;

    if (!data_.empty() && !WriteKey(hkey, is_global_, true, data_))
        goto failure;

    CloseKey(hkey);
    return true;

failure:
    CloseKey(hkey);
    return false;
}

void LoadSavedAccounts(std::vector<Account> *loadedAccounts) {
    FILE *f = fopen(kGenshinImpactDbFileName, "r");
    struct {
        char name[kMaxDisplayNameLength];
        int isGlobal;
        char account[128];
        char userData[128 * 1024];
    } accnt;

    if (!f)
        return;

    while (fscanf(f, "%s %d %s %s\n", accnt.name, &accnt.isGlobal, accnt.account, accnt.userData) == 4) {
        std::string display_name = accnt.name;
        std::vector<uint8_t> name;
        size_t len = strnlen(accnt.account, sizeof(accnt.account));
        name.resize(len + 1, 0);
        memcpy(name.data(), accnt.account, len);

        std::vector<uint8_t> data;
        len = strnlen(accnt.userData, sizeof(accnt.userData));
        data.resize(len + 1, 0);
        memcpy(data.data(), accnt.userData, len);
        Account account(accnt.isGlobal, display_name, name, data);

        loadedAccounts[accnt.isGlobal != 0 ? 0 : 1].push_back(account);
    }

    fclose(f);
}

void SaveAccounts(const std::vector<Account> *loadedAccounts) {
    FILE* f = fopen(kGenshinImpactDbFileName, "w");
    if (!f)
        return;
    for (int i = 0; i != 2; ++i)
        for (const auto &account : loadedAccounts[i])
           fprintf(f, "%s %d %s %s\n", account.display_name().c_str(),
                   account.is_global(),
                   (const char*)&account.name()[0],
                   (const char*)&account.data()[0]);
    fclose(f);
}
