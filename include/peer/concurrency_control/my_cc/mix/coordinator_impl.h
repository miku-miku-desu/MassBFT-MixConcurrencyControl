//
// Created by miku on 25-4-27.
//

#pragma once

#include "peer/concurrency_control/coordinator.h"
#include "peer/concurrency_control/my_cc/crdt/coordinator_impl.h"
#include "peer/concurrency_control/my_cc/deterministic/coordinator_impl.h"
#include "peer/concurrency_control/my_cc/serial/coordinator_impl.h"
#include "peer/concurrency_control/worker_fsm.h"

namespace peer::cc::mycc::mix {
  class MixWorkerFSM : public WorkerFSM {
    ReceiverState OnCreate() override {
      return ReceiverState::READY;
    }
    ReceiverState OnExecuteTransaction() override {
      return ReceiverState::FINISH_EXEC;
    }
    ReceiverState OnCommitTransaction() override {
      return ReceiverState::FINISH_COMMIT;
    }
    ReceiverState OnDestroy() override {
      return ReceiverState::EXITED;
    }
  };


  class MixCoordinator : public Coordinator<MixWorkerFSM, MixCoordinator> {
  public:
    bool init(std::shared_ptr<db::DBConnection> db) {
      int workerCount = this->workerList.size();
      deterministic = deterministic::DeterministicCoordinator::NewCoordinator(db, workerCount);
      crdt = crdt::mycrdt::CrdtCoordinator::NewCoordinator(db, workerCount);
      return true;
    }

    bool processSync(const auto& a, const auto& b) {
      return true;
    }

    bool processValidatedRequests(std::vector<std::unique_ptr<proto::Envelop>>& requests, std::vector<std::unique_ptr<proto::TxReadWriteSet>>& retRWSets, std::vector<std::byte>& retResults) override {
      auto requestCount = requests.size();

      deterministicTxn.clear();
      crdtTxn.clear();
      isCrdtTxn.clear();
      isCrdtTxn.resize(requestCount);

      retRWSets.resize(requestCount);
      retResults.resize(requestCount);

      // classify txn type
      for (int i = 0; i < (int)requestCount; i++) {
        auto& req = requests[i];
        auto txn = proto::Transaction::NewTransactionFromEnvelop(std::move(req));
        CHECK(txn != nullptr) << "cannot get txn from envelop";
        auto& userRequest = txn->getUserRequest();
        if (peer::crdt::chaincode::isCrdtChainCode(userRequest.getCCNameSV())) {
          crdtTxn.push_back(std::move(txn));
          isCrdtTxn[i] = true;
        } else {
          deterministicTxn.push_back(std::move(txn));
          isCrdtTxn[i] = false;
        }
      }

      // LOG(INFO) << "process deterministic txn: " << deterministicTxn.size() << " crdt txn: " << crdtTxn.size();

      // using each type coordinator to process txn
      if (!crdt->processValidatedRequests(crdtTxn)) {
        return false;
      }

      if (!deterministic->processValidatedRequests(deterministicTxn)) {
        return false;
      }

      for (int i = 0, c = 0, d = 0; i < (int)requestCount; i++) {
        retResults[i] = static_cast<std::byte>(false);
        if (isCrdtTxn[i]) {
          auto& txn = crdtTxn[c++];
          CHECK(txn != nullptr) << "except txn but nullptr";
          if (txn->getExecutionResult() == proto::Transaction::ExecutionResult::COMMIT) {
            retResults[i] = static_cast<std::byte>(true);
          }
          auto ret = proto::Transaction::DestroyTransaction(std::move(txn));
          requests[i] = std::move(ret.first);
          retRWSets[i] = std::move(ret.second);
        } else {
          auto& txn = deterministicTxn[d++];
          CHECK(txn != nullptr) << "except txn but nullptr";
          if (txn->getExecutionResult() == proto::Transaction::ExecutionResult::COMMIT) {
            retResults[i] = static_cast<std::byte>(true);
          }
          auto ret = proto::Transaction::DestroyTransaction(std::move(txn));
          requests[i] = std::move(ret.first);
          retRWSets[i] = std::move(ret.second);
        }
      }
      return true;
    }

    friend class Coordinator;


  private:
    std::unique_ptr<deterministic::DeterministicCoordinator> deterministic;
    std::unique_ptr<crdt::mycrdt::CrdtCoordinator> crdt;
    std::vector<std::unique_ptr<proto::Transaction>> deterministicTxn;
    std::vector<std::unique_ptr<proto::Transaction>> crdtTxn;
    std::vector<bool> isCrdtTxn;
  };
}
