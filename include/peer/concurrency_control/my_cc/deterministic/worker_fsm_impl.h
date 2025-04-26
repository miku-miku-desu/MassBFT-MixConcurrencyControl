//
// Created by miku on 25-4-26.
//

#ifndef WORKER_FSM_IMPL_H
#  define WORKER_FSM_IMPL_H
#  include <braft/fsm_caller.h>

#  include "peer/chaincode/chaincode.h"
#  include "peer/concurrency_control/worker_fsm.h"
#  include "proto/transaction.h"
#  include "reserve_table.h"

#endif //WORKER_FSM_IMPL_H


namespace peer::cc::mycc::deterministic {

  class DeterministicFSM : public peer::cc::WorkerFSM {
  public:
    ReceiverState OnCreate() override {
      pthread_setname_np(pthread_self(), "aria_worker");
      return peer::cc::ReceiverState::READY;
    }

    ReceiverState OnDestroy() override {
      return peer::cc::ReceiverState::EXITED;
    }

    ReceiverState OnExecuteTransaction() override {
      DCHECK(dbc != nullptr && reserveTable != nullptr);
      for (auto& txn : txnList) {
        auto& userRequest = txn->getUserRequest();
        auto ccName = userRequest.getCCNameSV();
        auto cc = getOrCreateChaincode(ccName);
        auto ret = cc->InvokeChaincode(userRequest.getFuncNameSV(), userRequest.getArgs());
        txn->setRetValue(cc->reset(txn->getReads(), txn->getWrites()));
        if (ret != 0) {
          txn->setExecutionResult(proto::Transaction::ExecutionResult::ABORT_NO_RETRY);
          continue;
        }
        if (txn->getWrites().empty()) {
          txn->setExecutionResult(proto::Transaction::ExecutionResult::COMMIT);
          continue;
        }
        reserveTable->updateTable(txn->getReads(), txn->getWrites(), txn->getTransactionIdPtr());
      }
      return ReceiverState::FINISH_EXEC;
    }

    ReceiverState OnCommitTransaction() override {
      auto saveToDB = [&](db::PHMapConnection::WriteBatch* batch) {
        for (auto& txn : txnList) {
          auto result = txn->getExecutionResult();
          if (result == proto::Transaction::ExecutionResult::ABORT_NO_RETRY) {
            continue;
          }
          if (result == proto::Transaction::ExecutionResult::COMMIT) {
            continue;
          }

          auto dep = reserveTable->getDependence(txn->getReads(), txn->getWrites(), txn->getTransactionId());
          if (dep.waw) {
            txn->setExecutionResult(proto::Transaction::ExecutionResult::ABORT);
            continue;
          } else if (dep.war && dep.raw) {
            txn->setExecutionResult(proto::Transaction::ExecutionResult::ABORT);
            continue;
          } else {
            txn->setExecutionResult(proto::Transaction::ExecutionResult::COMMIT);
            for (const auto& write : txn->getWrites()) {
              auto key = write->getKeySV();
              auto value = write->getValueSV();
              if (value.empty()) {
                batch->Delete({key.data(), key.size()});
              } else {
                batch->Put({key.data(), key.size()}, {value.data(), value.size()});
              }
            }
          }
        }
        return true;
      };
      if (!dbc->syncWriteBatch(saveToDB)) {
        LOG(ERROR) << "db->syncWriteBatch failed";
      }
      return ReceiverState::FINISH_COMMIT;
    }

    inline void setDB(std::shared_ptr<db::DBConnection> db) {dbc = std::move(db); }
    [[nodiscard]] inline db::DBConnection* getDB() const { return dbc.get(); }
    [[nodiscard]] std::vector<std::unique_ptr<proto::Transaction>>& getMutableTxnList() {return txnList; }
    void setReserveTable(std::shared_ptr<ReserveTable> reserve_table) {reserveTable = std::move(reserve_table); }

  protected:
    inline peer::chaincode::Chaincode* getOrCreateChaincode(std::string_view ccName) {
      auto result = ccList.find(ccName);
      if (result == ccList.end()) {
        auto orm = peer::chaincode::ORM::NewORMFromDBInterface(dbc);
        auto cc = peer::chaincode::NewChaincodeByName(ccName, std::move(orm));
        CHECK(cc != nullptr) << "chaincode " << ccName << " not exists";
        auto& ptr = *cc;
        ccList[ccName] = std::move(cc);
        return &ptr;
      } else {
        return result->second.get();
      }
    }

  private:
    std::shared_ptr<db::DBConnection> dbc;
    util::MyFlatHashMap<std::string, std::unique_ptr<peer::chaincode::Chaincode>> ccList;
    std::vector<std::unique_ptr<proto::Transaction>> txnList;
    std::shared_ptr<ReserveTable> reserveTable;
  };
}
