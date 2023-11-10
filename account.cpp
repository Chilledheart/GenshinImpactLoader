// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#include "account.hpp"

#include <stdio.h>
#include <windows.h>

#include "helper.hpp"

#include <leveldb/db.h>
#include <nlohmann/json.hpp>

static const char kGenshinImpactDbFileName[] = "GenshinImpactLoader.dat";
static const char kGenshinImpactLevelDbFileName[] = "GenshinImpactLoader.db";

using json = nlohmann::json;

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
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, kGenshinImpactLevelDbFileName, &db);
    if (!status.ok()) {
        // "Unable to open/create db" status.ToString()
        return;
    }

    // Iterate over each item in the database and print them
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        json root;
        uint64_t id = std::stoull(it->key().ToString());
        // TODO avoid memcopy
        std::string slice = it->value().ToString();
        root = json::parse(slice, nullptr, false);
        if (root.is_discarded() || !root.is_object()) {
            // err "Unable to parse key it->key()"
            continue;
        }
        if (root.contains("display_name") && root["display_name"].is_string() &&
            root.contains("is_global") && root["is_global"].is_boolean() &&
            root.contains("name") && root["name"].is_string() &&
            root.contains("data") && root["data"].is_string()) {
            std::string c_name = root["name"].get<std::string>();
            std::string c_data = root["data"].get<std::string>();

            std::vector<uint8_t> name;
            size_t len = c_name.size();
            name.resize(len + 1, 0);
            memcpy(name.data(), c_name.c_str(), len);

            std::vector<uint8_t> data;
            len = c_data.size();
            data.resize(len + 1, 0);
            memcpy(data.data(), c_data.c_str(), len);

            Account account(id,
                            root["is_global"].get<bool>(),
                            root["display_name"].get<std::string>(),
                            name, data);
            loadedAccounts[account.is_global() != 0 ? 0 : 1].push_back(account);
        } else {
            // err non-parsable data
        }
    }

    if (!it->status().ok()) {
        // err db non-parsable data
    }

    delete it;
    delete db;
}

void SaveAccounts(const std::vector<Account> *loadedAccounts) {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, kGenshinImpactLevelDbFileName, &db);
    if (!status.ok()) {
        // "Unable to open/create db" status.ToString()
        return;
    }

    leveldb::WriteOptions writeOptions;
    for (int i = 0; i != 2; ++i) {
        for (const auto& account : loadedAccounts[i]) {
          std::string name = (const char*)&account.name()[0];
          std::string data = (const char*)&account.data()[0];
          json root;
          root["id"] = account.id();
          root["display_name"] = account.display_name();
          root["is_global"] = account.is_global();
          root["name"] = name;
          root["data"] = data;

          db->Put(writeOptions, std::to_string(account.id()), root.dump());
        }
    }

    delete db;
}

void LoadSavedAccounts_Old(std::vector<Account> *loadedAccounts) {
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
        Account account(Rand<uint64_t>(), accnt.isGlobal, display_name, name, data);

        loadedAccounts[accnt.isGlobal != 0 ? 0 : 1].push_back(account);
    }

    fclose(f);
}
