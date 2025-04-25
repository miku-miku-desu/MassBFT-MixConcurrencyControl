//
// Created by miku on 25-4-21.
//

#ifndef COORDINATOR_IMPL_H
#  define COORDINATOR_IMPL_H
#  include <braft/fsm_caller.h>

#  include "peer/chaincode/chaincode.h"
#  include "peer/concurrency_control/coordinator.h"

#endif //COORDINATOR_IMPL_H


namespace peer::cc::mycc::serial {

  // empty
  class SerialWorkerFSM : public peer::cc::WorkerFSM {
    ReceiverState OnCreate() override {
      return ReceiverState::READY;
    }

    ReceiverState OnDestroy() override {
      return ReceiverState::EXITED;
    }

    ReceiverState OnExecuteTransaction() override {
      return ReceiverState::FINISH_EXEC;
    }

    ReceiverState OnCommitTransaction() override {
      return ReceiverState::FINISH_COMMIT;
    }
  };

  class SerialCoordinator : public peer::cc::Coordinator<SerialWorkerFSM, SerialCoordinator> {
  public:
    bool init(const std::shared_ptr<db::DBConnection>& db) {
      dbc = db;
      return true;
    }

    bool processSync(const auto& a, const auto& b) {
      return true;
    }

    bool processValidatedRequests(std::vector<std::unique_ptr<proto::Envelop>>& requests,
                                  std::vector<std::unique_ptr<proto::TxReadWriteSet>>& retRWSets,
                                  std::vector<std::byte>& retResults) override {
      auto txnCount = requests.size();
      retResults.resize(txnCount);
      retRWSets.resize(txnCount);

      txnList.clear();
      txnList.reserve(txnCount);
      // process each request
      for (auto&it : requests) {
        auto txn = proto::Transaction::NewTransactionFromEnvelop(std::move(it));
        CHECK(txn != nullptr) << "Transaction creation failed";
        txnList.push_back(std::move(txn));
      }

      for (auto& txn : txnList) {
        // process txn
        auto& userRequest = txn->getUserRequest();
        auto ccName = userRequest.getCCNameSV();
        auto cc = getOrCreateChaincode(ccName);
        auto result = cc->InvokeChaincode(userRequest.getFuncNameSV(), userRequest.getArgs());
        txn->setRetValue(cc->reset(txn->getReads(), txn->getWrites()));
        if (result != 0) {
          txn->setExecutionResult(proto::Transaction::ExecutionResult::ABORT_NO_RETRY);
          continue;
        }
        if (txn->getWrites().empty()) {
          txn->setExecutionResult(proto::Transaction::ExecutionResult::COMMIT);
          continue;
        }

        auto writeToDB = [&](peer::db::PHMapConnection::WriteBatch* batch) -> bool {
          txn->setExecutionResult(proto::Transaction::ExecutionResult::COMMIT);
          for (const auto& kv : txn->getWrites()) {
            auto& key = kv->getKeySV();
            auto& value = kv->getValueSV();
            if (value.empty()) {
              batch->Delete({key.data(), key.size()});
            } else {
              batch->Put({key.data(), key.size()}, {value.data(), value.size()});
            }
          }
          return true;
        };

        if (!dbc->syncWriteBatch(writeToDB)) {
          LOG(ERROR) << "sync to db failed";
        }
      }

      for (int i = 0; i < (int)requests.size(); i++) {
        // push back txn and result
        retResults[i] = static_cast<std::byte>(false);
        // if (txn == nullptr) {
        //   continue;
        // }
        auto& txn = txnList[i];
        if (txn->getExecutionResult() == proto::Transaction::ExecutionResult::COMMIT) {
          retResults[i] = static_cast<std::byte>(true);
        }

        auto ret = proto::Transaction::DestroyTransaction(std::move(txn));
        requests[i] = std::move(ret.first);
        retRWSets[i] = std::move(ret.second);
      }

      return true;
    }

    friend class Coordinator;


  protected:

    SerialCoordinator() = default;

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
    std::vector<std::unique_ptr<proto::Transaction>> txnList;
    util::MyFlatHashMap<std::string, std::unique_ptr<peer::chaincode::Chaincode>> ccList;
    std::shared_ptr<peer::db::DBConnection> dbc;
  };
}
