//
// Created by miku on 25-4-26.
//

#ifndef COORDINATOR_IMPL_H
#  define COORDINATOR_IMPL_H
#  include "peer/concurrency_control/coordinator.h"
#  include "worker_fsm_impl.h"

#endif //COORDINATOR_IMPL_H


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
