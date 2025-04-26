//
// Created by miku on 25-4-26.
//
#pragma once

#  include "peer/chaincode/crdt/crdt_chaincode.h"
#  include "peer/chaincode/crdt/crdt_orm.h"
#  include "peer/concurrency_control/worker_fsm.h"
#  include "proto/transaction.h"


namespace peer::cc::crdt::mycrdt {
  class crdtFSM : public peer::cc::WorkerFSM {
  protected:
    ReceiverState OnDestroy() override {
      return ReceiverState::EXITED;
    }

    ReceiverState OnCreate() override {
      pthread_setname_np(pthread_self(), "aria_worker");
      return ReceiverState::READY;
    }

    ReceiverState OnExecuteTransaction() override {
      for (auto& txn : txnList) {
        auto& userRequest = txn->getUserRequest();
        auto ccName = userRequest.getCCNameSV();
        auto cc = getOrCreateChaincode(ccName);
        // crdt chaincode will write database on InvokeChaincode function
        auto ret = cc->InvokeChaincode(userRequest.getFuncNameSV(), userRequest.getArgs());
        txn->setRetValue(cc->reset());
        if (ret != 0) {
          txn->setExecutionResult(proto::Transaction::ExecutionResult::ABORT_NO_RETRY);
          continue;
        }
        txn->setExecutionResult(proto::Transaction::ExecutionResult::COMMIT);
      }
      return ReceiverState::FINISH_EXEC;
    }

    ReceiverState OnCommitTransaction() override {
      return ReceiverState::FINISH_COMMIT;
    }

    inline peer::crdt::chaincode::CrdtChaincode* getOrCreateChaincode(std::string_view ccName) {
      auto it = ccList.find(ccName);
      if (it == ccList.end()) {
        auto orm = std::make_unique<peer::crdt::chaincode::CrdtORM>(dbc);
        auto cc = peer::crdt::chaincode::NewChaincodeByName(ccName, std::move(orm));
        CHECK(cc != nullptr) << "can not get chaincode";
        auto& pointer = *cc;
        ccList[ccName] = std::move(cc);
        return &pointer;
      } else {
        return it->second.get();
      }
    }

  public:
    inline void setDBShim(std::shared_ptr<peer::crdt::chaincode::DBShim> db) {dbc = std::move(db); }

    [[nodiscard]] std::vector<std::unique_ptr<proto::Transaction>>& getMutableTxnList() {return txnList; }

  private:
    std::vector<std::unique_ptr<proto::Transaction>> txnList;
    util::MyFlatHashMap<std::string, std::unique_ptr<peer::crdt::chaincode::CrdtChaincode>> ccList;
    std::shared_ptr<peer::crdt::chaincode::DBShim> dbc;
  };
}
