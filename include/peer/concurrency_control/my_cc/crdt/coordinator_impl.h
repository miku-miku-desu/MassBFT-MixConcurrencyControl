//
// Created by miku on 25-4-26.
//
# pragma once

#  include "peer/concurrency_control/coordinator.h"
#  include "worker_fsm_impl.h"
#  include "peer/db/db_interface.h"



namespace peer::cc::crdt::mycrdt {

  class CrdtCoordinator : public Coordinator<crdtFSM, CrdtCoordinator> {
  public:
    bool init(const std::shared_ptr<peer::db::DBConnection>& dbc) {
      auto dbShim = std::make_shared<peer::crdt::chaincode::DBShim>(dbc);

      for (auto& it : this->fsmList) {
        it->setDBShim(dbShim);
      }
      return true;
    }

    bool processSync(const auto& afterStart, const auto& afterCommit) {
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
      return true;
    }

    CrdtCoordinator() = default;
  };
}

