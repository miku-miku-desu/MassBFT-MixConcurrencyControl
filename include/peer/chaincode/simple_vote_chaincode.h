//
// Created by miku on 25-4-26.
//

#pragma once

#  include "chaincode.h"
#  include "client/core/generator/generator.h"
#  include "client/crdt/crdt_property.h"
#  include "common/property.h"


namespace peer::chaincode {
  // transplant from crdt chaincode
  class SimpleVote : public Chaincode {
  public:
    explicit SimpleVote(std::unique_ptr<ORM> orm_) : Chaincode(std::move(orm_)) {}

    int InitDatabase() override {
      client::core::GetThreadLocalRandomGenerator()->seed(0);
      auto property = util::Properties::GetProperties();
      auto crdtProperty = client::crdt::CrdtProperties::NewFromProperty(*property);
      auto candidate = crdtProperty->getCandidateCount();
      for (int i = 0; i < candidate; i++) {
        auto name = std::to_string(i);
        std::string value;
        zpp::bits::out out(value);
        if (zpp::bits::failure(out(int(0)))) {
          LOG(ERROR) << "cannot serialize int 0";
        }
        orm->put(name, value);
      }
      return 0;
    }

    int InvokeChaincode(std::string_view funcNameSV, std::string_view argSV) override {
      if (funcNameSV == client::crdt::StaticConfig::VOTING_GET) {
        return Get(std::string(argSV));
      }
      if (funcNameSV == client::crdt::StaticConfig::VOTING_VOTE) {
        zpp::bits::in in(argSV);
        std::string key;
        int count;
        if (zpp::bits::failure(in(key, count))) {
          return -1;
        }
        return Vote(key, count);
      }
      return -1;
    }

  protected:
    int Vote(const std::string& candidate, int count) {
      int oldCount;
      std::string_view rawValue;
      if (!this->orm->get(candidate, &rawValue)) {
        return -2;
      }
      zpp::bits::in in(rawValue);
      if (zpp::bits::failure(in(oldCount))) {
        return -3;
      }
      std::string value;
      zpp::bits::out out(value);
      if (zpp::bits::failure(out(oldCount + count))) {
        return -4;
      }
      this->orm->put(candidate, value);
      return 0;
    }

    int Get(const std::string& candidate) {
      std::string_view value;
      if (this->orm->get(candidate, &value)) {
        this->orm->setResult(std::move(std::string(value)));
        return 0;
      }
      return -1;
    }
  };
}
