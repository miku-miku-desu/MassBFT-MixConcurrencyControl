//
// Created by miku on 25-4-26.
//
# pragma once

#include "peer/chaincode/crdt/pncounter.h"
#include "peer/concurrency_control/coordinator.h"
#include "peer/db/db_interface.h"
#include "worker_fsm_impl.h"

namespace peer::cc::crdt::mycrdt {

  class CrdtCoordinator : public Coordinator<crdtFSM, CrdtCoordinator> {
  public:
    bool init(const std::shared_ptr<peer::db::DBConnection>& dbc) {
      this->dbc = dbc;
      counters = std::make_shared<peer::crdt::chaincode::type::PNCounterList>();
      auto dbShim = std::make_shared<peer::crdt::chaincode::DBShim>(dbc, counters);

      for (auto& it : this->fsmList) {
        it->setDBShim(dbShim);
      }
      return true;
    }

    using Coordinator::processValidatedRequests;

    bool processValidatedRequests(std::vector<std::unique_ptr<proto::Transaction>>& transactions) {
      const auto workerCount = this->workerList.size();

      auto afterStart = [&](const auto& worker, auto& fsm) {
        auto& fsmTxnList = fsm.getMutableTxnList();
        fsmTxnList.clear();
        fsmTxnList.reserve(transactions.size() / workerCount + 1);
        for (int i = worker.getId(); i < (int)transactions.size(); i += workerCount) {
          fsmTxnList.push_back(std::move(transactions[i]));
        }
      };

      auto afterCommit = [&](const auto& worker, auto& fsm) {
        auto& fsmTxnList = fsm.getMutableTxnList();
        auto id = worker.getId();
        for (int i = id, j = 0; i < (int)transactions.size(); i+= workerCount) {
          auto& txn = fsmTxnList[j++];
          CHECK(txn != nullptr) << "unknown nullptr txn";
          transactions[i] = std::move(txn);
        }
      };

      return this->processSync(afterStart, afterCommit);
    }

    bool processSync(const auto& afterStart, const auto& afterCommit) {
      counters->reset();

      auto ret = processParallel(InvokerCommand::START, ReceiverState::READY, afterStart);
      if (!ret) {
        LOG(ERROR) << "START failed";
        return false;
      }
      ret = processParallel(InvokerCommand::EXEC, ReceiverState::FINISH_EXEC, afterCommit);
      if (!ret) {
        LOG(ERROR) << "EXEC failed";
        return false;
      }
      // save counters to db
      if (!counters->saveToDB(dbc)) {
        LOG(ERROR) << "sync to db failed";
        return false;
      }
      return true;
    }

    CrdtCoordinator() = default;

  private:
    std::shared_ptr<peer::crdt::chaincode::type::PNCounterList> counters;
    std::shared_ptr<db::DBConnection> dbc;
  };
}

