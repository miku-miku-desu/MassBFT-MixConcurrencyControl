//
// Created by peng on 8/14/23.
//

#pragma once

#include "peer/db/db_interface.h"
#include "proto/read_write_set.h"
#include "common/phmap.h"
#include "zpp_bits.h"
#include <functional>
#include "peer/chaincode/crdt/pncounter.h"

namespace peer::crdt::chaincode {
    class DBShim {
    public:
        explicit DBShim(std::shared_ptr<db::DBConnection> db, std::shared_ptr<peer::crdt::chaincode::type::PNCounterList> counters_) : _db(std::move(db)), counters(std::move(counters_)) { }
        explicit DBShim(std::shared_ptr<db::DBConnection> db) : _db(std::move(db)) {}

        [[nodiscard]] inline bool get(const std::string& key, std::string& value) {
            std::shared_lock lock(mutexMap[key]);
            auto ret = _db->get(key, &value);
            // empty value is marked deleted
            if (!ret || value.empty()) {
                return false;
            }
            return true;
        }

        inline bool put(const std::string& key, const std::function<bool(std::string& value)>& callback) {
            std::unique_lock lock(mutexMap[key]);
            std::string value;
            auto ret = _db->get(key, &value);
            if (!ret) {
                value.clear();
            }
            if (!callback(value)) {
                return false;
            }
            return _db->asyncPut(key, std::move(value));
        }

        inline bool add(const std::string& key, int value) {
          return counters->apply(type::Operator::ADD, key, value);
        }

        inline bool sub(const std::string& key, int value) {
          return counters->apply(type::Operator::REDUCE, key, value);
        }

        inline bool del(const std::string& key) {
            std::unique_lock lock(mutexMap[key]);
            return _db->asyncDelete(key);
        }



    private:
        std::shared_ptr<db::DBConnection> _db;
        util::MyNodeHashMap<std::string, std::shared_mutex, std::mutex> mutexMap;
        std::shared_ptr<peer::crdt::chaincode::type::PNCounterList> counters;
    };

    class CrdtORM {
    public:
        explicit CrdtORM(std::shared_ptr<DBShim> db) : db(std::move(db)) { }

        [[nodiscard]] inline bool get(const std::string& key, std::string& value) {
            return db->get(key, value);
        }

        inline bool put(const std::string& key, const std::function<bool(std::string& value)>& callback) {
            return db->put(key, callback);
        }

        inline bool del(const std::string& key) {
            return db->del(key);
        }

        inline bool add(const std::string& key, int value) {
          return db->add(key, value);
        }

        inline bool sub(const std::string& key, int value) {
          return db->sub(key, value);
        }

        // set the return string
        inline void setResult(auto&& result_) { result = std::forward<decltype(result_)>(result_); }

        // return the return string
        inline std::string reset() {
            return std::move(result);
        }

    private:
        std::shared_ptr<DBShim> db;
        std::string result;
    };
}