// SPDX-License-Identifier: MIT
/* Copyright (c) 2023 Chilledheart  */

#ifndef _ACCOUNT_HPP
#define _ACCOUNT_HPP

#include <string>
#include <vector>
#include <stdint.h>

#include "helper.hpp"

class Account {
  public:
    Account(bool is_global, const std::string& display_name)
      : id_(Rand<uint64_t>()), is_global_(is_global), display_name_(display_name) {}

    Account(uint64_t id, bool is_global, const std::string& display_name,
            const std::vector<uint8_t> &name,
            const std::vector<uint8_t> &data)
      : id_(id), is_global_(is_global), display_name_(display_name),
        name_(name), data_(data) {}

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

    bool Load();
    bool Save() const;

  private:
    uint64_t id_;
    bool is_global_;
    std::string display_name_;
    std::vector<uint8_t> name_;
    std::vector<uint8_t> data_;
};

// in order 0-> Global 1-> CN
void LoadSavedAccounts(std::vector<Account> *loadedAccounts);
// in order 0-> Global 1-> CN
void SaveAccounts(const std::vector<Account> *loadedAccounts);

void LoadSavedAccounts_Old(std::vector<Account> *loadedAccounts);

constexpr size_t kMaxDisplayNameLength = 128U;

#endif //  _ACCOUNT_HPP
