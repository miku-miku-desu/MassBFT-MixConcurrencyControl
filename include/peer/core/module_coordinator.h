//
// Created by user on 23-5-16.
//

#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "common/async_serial_executor.h"
#include "peer/concurrency_control/my_cc/crdt/coordinator_impl.h"
#include "peer/concurrency_control/my_cc/deterministic/coordinator_impl.h"
#include "peer/concurrency_control/my_cc/mix/coordinator_impl.h"
#include "peer/concurrency_control/my_cc/serial/coordinator_impl.h"
#include "peer/db/db_interface.h"

namespace util {
    class Properties;
    struct NodeConfig;
}

namespace peer {
    class MRBlockStorage;
    class BlockLRUCache;
    namespace consensus {
        class BlockOrderInterface;
    }
    namespace cc {
        class CoordinatorImpl;
        namespace crdt {
            class CRDTCoordinator;
        }
        namespace serial {
            class SerialCoordinator;
        }
    }
}

namespace peer::core {
    class ModuleFactory;
    struct BFTController;

    class ModuleCoordinator {
    public:
        // Uncomment this line to enable CRDT chaincode
        // using ChaincodeType = peer::cc::crdt::CRDTCoordinator;
        // Uncomment this line to enable traditional chaincode
        // using ChaincodeType = peer::cc::CoordinatorImpl;
        // Uncomment this line to enable serial exec chaincode
        // using ChaincodeType = peer::cc::serial::SerialCoordinator;
        // using ChaincodeType = peer::cc::mycc::serial::SerialCoordinator;
        // using ChaincodeType = peer::cc::mycc::deterministic::DeterministicCoordinator;
        // using ChaincodeType = peer::cc::crdt::mycrdt::CrdtCoordinator;
        using ChaincodeType = peer::cc::mycc::mix::MixCoordinator;

        static std::unique_ptr<ModuleCoordinator> NewModuleCoordinator(const std::shared_ptr<util::Properties>& properties);

        bool initChaincodeData(const std::string& ccName);

        bool initCrdtChaincodeData(const std::string& ccName);

        ~ModuleCoordinator();

        ModuleCoordinator(const ModuleCoordinator&) = delete;

        ModuleCoordinator(ModuleCoordinator&&) noexcept = delete;

        [[nodiscard]] auto& getModuleFactory() const { return *_moduleFactory; }

        bool startInstance();

        void waitInstanceReady() const;

        [[nodiscard]] std::shared_ptr<peer::db::DBConnection> getDBHandle() const { return _db; }

    protected:
        ModuleCoordinator() = default;

        void contentLeaderReceiverLoop();

        bool onConsensusBlockOrder(int regionId, int blockId);

    private:
        // for subscriber
        int _subscriberId = -1;
        std::atomic<bool> _running = true;
        std::unique_ptr<std::thread> _subscriberThread;
        // for moduleFactory
        std::unique_ptr<ModuleFactory> _moduleFactory;
        // other components
        std::shared_ptr<::peer::MRBlockStorage> _contentStorage;
        std::unique_ptr<::peer::consensus::BlockOrderInterface> _gbo;
        std::unique_ptr<BFTController> _localContentBFT;
        // for debug
        std::shared_ptr<util::NodeConfig> _localNode;
        // for concurrency control
        std::shared_ptr<peer::db::DBConnection> _db;
        std::unique_ptr<ChaincodeType> _cc;
        util::AsyncSerialExecutor _serialExecutor;
        // for user rpc
        std::shared_ptr<::peer::BlockLRUCache> _userRPCNotifier;
    };
}