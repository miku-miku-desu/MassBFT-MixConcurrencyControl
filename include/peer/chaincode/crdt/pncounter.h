//
// Created by miku on 25-5-1.
//

#pragma once

#include <zpp_bits.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "common/phmap.h"
#include "peer/db/db_interface.h"

namespace peer::crdt::chaincode::type {

  enum class Operator {
    ADD,
    REDUCE
  };

  class PNCounter {

  public:
    PNCounter() {p = 0; n = 0;}

    PNCounter(const PNCounter&) = delete;
    PNCounter(PNCounter&&) = delete;


    bool apply(Operator operator_, int value) {
      if (operator_ == Operator::ADD) {
        p += value;
        return true;
      } else if (operator_ == Operator::REDUCE) {
        n += value;
        return true;
      }
      return false;
    }

    int value() {
      return p - n;
    }

  private:
    int p;
    int n;
  };

  class PNCounterList {

  public:
    PNCounterList() = default;
    PNCounterList(const PNCounterList&) = delete;
    PNCounterList(PNCounterList&&) = delete;

    bool apply(Operator operator_, const std::string& key, int value) {
      std::unique_lock lock(mutexMap[key]);
      if (list.contains(key)) {
        return list[key]->apply(operator_, value);
      } else {
        auto v = std::make_unique<PNCounter>();
        auto result = v->apply(operator_, value);
        list[key] = std::move(v);
        return result;
      }
    }

    int value(const std::string& key) {
      std::shared_lock lock(mutexMap[key]);
      if (list.contains(key)) {
        return list[key]->value();
      } else {
        auto v = std::make_unique<PNCounter>();
        auto d = v->value();
        list[key] = std::move(v);
        return d;
      }
    }

    void reset() {
      list.clear();
    }

    bool saveToDB(std::shared_ptr<db::DBConnection> dbc) {
      for (const auto& it : list) {
        int value = it.second->value();
        std::string rawValue;
        auto ret = dbc->get(it.first, &rawValue);
        if (!ret) {
          rawValue.clear();
        }

        int oldCount;
        zpp::bits::in in(rawValue);
        if (zpp::bits::failure(in(oldCount))) {
          return false;
        }

        zpp::bits::out out(rawValue);
        if (zpp::bits::failure(out(oldCount + value))) {
          return false;
        }
        if (!dbc->asyncPut(it.first, rawValue)) {
          return false;
        }
      }
      return true;
    }

  private:
    std::unordered_map<std::string, std::unique_ptr<PNCounter>> list;
    util::MyNodeHashMap<std::string, std::shared_mutex, std::mutex> mutexMap;
  };

}
