//
// Created by miku on 25-4-26.
//

#pragma once

#  define COORDINATOR_IMPL_H
#  include "peer/concurrency_control/coordinator.h"
#  include "worker_fsm_impl.h"



namespace peer::cc::mycc::deterministic {

  class DeterministicCoordinator : public Coordinator<DeterministicFSM, DeterministicCoordinator> {
  public:
    bool init(const std::shared_ptr<db::DBConnection>& db) {
      auto table = std::make_shared<ReserveTable>();
      reserveTable = table;
      for (auto& it : this->fsmList) {
        it->setDB(db);
        it->setReserveTable(table);
      }
      return true;
    }

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
      reserveTable->reset();
      auto ret = processParallel(InvokerCommand::START, ReceiverState::READY, afterStart);
      if (!ret) {
        LOG(ERROR) << "START failed";
        return false;
      }
      ret = processParallel(InvokerCommand::EXEC, ReceiverState::FINISH_EXEC, nullptr);
      if (!ret) {
        LOG(ERROR) << "EXEC failed";
        return false;
      }
      ret = processParallel(InvokerCommand::COMMIT, ReceiverState::FINISH_COMMIT, afterCommit);
      if (!ret) {
        LOG(ERROR) << "COMMIT failed";
        return false;
      }
      return true;
    }

    friend class Coordinator;

  private:
    std::shared_ptr<ReserveTable> reserveTable{};
  };
}
