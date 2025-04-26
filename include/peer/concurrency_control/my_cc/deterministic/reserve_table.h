//
// Created by miku on 25-4-26.
//

#ifndef RESERVE_TABLE_H
#  define RESERVE_TABLE_H
#  include <google/protobuf/stubs/mutex.h>

#  include "common/phmap.h"
#  include "proto/transaction.h"

#endif //RESERVE_TABLE_H


namespace peer::cc::mycc::deterministic {
  class ReserveTable {
  public:
    class Dependence {
    public:
      bool waw = false;
      bool war = false;
      bool raw = false;
    };

    ReserveTable() = default;

    ~ReserveTable() = default;

    ReserveTable(const ReserveTable&) = delete;

    ReserveTable(ReserveTable&&) = delete;

    void reset() {
      readTable.clear();
      writeTable.clear();
    }

    void updateTable(const proto::KVList& reads, const proto::KVList& writes, std::shared_ptr<const proto::tid_type> txn_id) {
      for (const auto& read : reads) {
        auto func = [&](MutexTable::value_type& value) {
          if (proto::CompareTID(*value.second, *txn_id) <= 0) {
            return;
          }
          value.second = txn_id;
        };
        readTable.try_emplace_l(read->getKeySV(), func, txn_id);
      }
      for (const auto& write : writes) {
        auto func = [&](MutexTable::value_type& value) {
          if (proto::CompareTID(*value.second, *txn_id) <= 0) {
            return;
          }
          value.second = txn_id;
        };
        writeTable.try_emplace_l(write->getKeySV(), func, txn_id);
      }
    }

    [[nodiscard]] Dependence getDependence(const proto::KVList& reads, const proto::KVList& writes, const proto::tid_type& txn_id) const {
      Dependence dep;

      for (const auto& write : writes) {
        writeTable.if_contains_unsafe(write->getKeySV(), [&](const MutexTable::value_type& value) {
          if (proto::CompareTID(*value.second, txn_id) < 0) {
            dep.waw = true;
          }
        });
        if (dep.waw) {
          break;
        }
      }

      for (const auto& write : writes) {
        readTable.if_contains_unsafe(write->getKeySV(), [&](const MutexTable::value_type& value) {
          if (proto::CompareTID(*value.second, txn_id) < 0) {
            dep.war = true;
          }
        });
        if (dep.war) {
          break;
        }
      }

      for (const auto& read : reads) {
        writeTable.if_contains_unsafe(read->getKeySV(), [&](const MutexTable::value_type& value) {
          if (proto::CompareTID(*value.second, txn_id) < 0) {
            dep.raw = true;
          }
        });
        if (dep.raw) {
          break;
        }
      }

      return dep;
    }


  private:
    using MutexTable = util::MyFlatHashMap<std::string_view, std::shared_ptr<const proto::tid_type>, std::mutex>;

    // using for dependence analysis
    MutexTable readTable;
    MutexTable writeTable;
  };
}
