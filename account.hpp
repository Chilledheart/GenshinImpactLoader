// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#ifndef _ACCOUNT_HPP
#define _ACCOUNT_HPP

#include <string>
#include <vector>
#include <stdint.h>

#include "helper.hpp"

#include <nlohmann/json.hpp>

class Account {
  public:
    Account(bool is_global, const std::string& display_name)
      : id_(Rand<uint64_t>()), is_global_(is_global), display_name_(display_name) {}

    Account(uint64_t id, bool is_global, const std::string& display_name,
            const std::vector<uint8_t> &name,
            const std::vector<uint8_t> &data)
      : id_(id), is_global_(is_global), display_name_(display_name),
        name_(name), data_(data) {
        if (!data.empty()) {
            std::string slice = reinterpret_cast<const char*>(&data[0]);
            data_json_ = nlohmann::json::parse(slice, nullptr, false);
        }
    }

    uint64_t id() const {
      return id_;
    }

    const std::string& display_name() const {
      return display_name_;
    }

    const bool is_global() const {
      return is_global_;
    }

    const std::vector<uint8_t>& name() const {
      return name_;
    }

    const std::vector<uint8_t>& data() const {
      return data_;
    }

    const nlohmann::json& data_json() const {
      return data_json_;
    }

    [[nodiscard]]
    bool Load();

    [[nodiscard]]
    bool Save() const;

  private:
    uint64_t id_;
    bool is_global_;
    std::string display_name_;
    std::vector<uint8_t> name_;
    std::vector<uint8_t> data_;

    nlohmann::json data_json_;
};

namespace leveldb {
class DB;
}

leveldb::DB* OpenDb(const std::string& db_path);
void CloseDb(leveldb::DB* db);

// in order 0-> Global 1-> CN
void LoadSavedAccounts(leveldb::DB* db, std::vector<Account> *loadedAccounts);

[[nodiscard]]
bool WipeAccountToDb(leveldb::DB* db, const Account &account);
[[nodiscard]]
bool SaveAccountToDb(leveldb::DB* db, const Account &account);

void LoadSavedAccounts_Old(std::vector<Account> *loadedAccounts);

constexpr size_t kMaxDisplayNameLength = 128U;

#endif //  _ACCOUNT_HPP
