//
// Created by user on 23-3-7.
//

#include "peer/chaincode/chaincode.h"

#include "client/crdt/crdt_engine.h"
#include "client/ycsb/ycsb_helper.h"
#include "peer/chaincode/crdt/crdt_chaincode.h"
#include "peer/chaincode/hash_chaincode.h"
#include "peer/chaincode/simple_session_store.h"
#include "peer/chaincode/simple_transfer.h"
#include "peer/chaincode/simple_vote_chaincode.h"
#include "peer/chaincode/small_bank_chaincode.h"
#include "peer/chaincode/tpcc_chaincode.h"
#include "peer/chaincode/ycsb_row_level.h"

namespace peer::chaincode {
    std::unique_ptr<Chaincode> NewChaincodeByName(std::string_view ccName, std::unique_ptr<ORM> orm) {
        if (ccName == client::ycsb::InvokeRequestType::YCSB) {
            // return std::make_unique<peer::chaincode::YCSBChaincode>(std::move(orm));
            return std::make_unique<peer::chaincode::YCSBRowLevel>(std::move(orm));
        }
        if (ccName == client::tpcc::InvokeRequestType::TPCC) {
            return std::make_unique<peer::chaincode::TPCCChaincode>(std::move(orm));
        }
        if (ccName == client::small_bank::InvokeRequestType::SMALL_BANK) {
            return std::make_unique<peer::chaincode::SmallBankChaincode>(std::move(orm));
        }
        if (ccName == client::crdt::StaticConfig::VOTING_CHAINCODE_NAME) {
          return std::make_unique<peer::chaincode::SimpleVote>(std::move(orm));
        }
        if (ccName == "transfer") {
            return std::make_unique<peer::chaincode::SimpleTransfer>(std::move(orm));
        }
        if (ccName == "session_store") {
            return std::make_unique<peer::chaincode::SimpleSessionStore>(std::move(orm));
        }
        if (ccName == "hash_chaincode") {
            return std::make_unique<peer::chaincode::HashChaincode>(std::move(orm));
        }
        LOG(ERROR) << "No matched chaincode found!";
        return nullptr;
    }
}