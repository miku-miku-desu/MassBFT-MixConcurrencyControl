//
// Created by miku on 25-4-28.
//

#pragma once

#include "client/core/generator/discrete_generator.h"
#include "client/core/workload.h"
#include "client/crdt/crdt_workload.h"
#include "client/ycsb/ycsb_engine.h"
#include "mix_property.h"

namespace client::mix {
  using MixDiscreteGenerator = core::DiscreteGenerator<ChaincodeType>;

  class MixWorkload : public core::Workload {
  public:
    MixWorkload() = default;

    void init(const util::Properties& n) override {
      auto p = MixProperties::NewFromProperty(n);
      chaincodeChooser = std::make_unique<MixDiscreteGenerator>();

      if (p->getVoteProportion() > 0) {
        chaincodeChooser->addValue(p->getVoteProportion(), ChaincodeType::VOTE);
      }
      if (p->getYCSBProportion() > 0) {
        chaincodeChooser->addValue(p->getYCSBProportion(), ChaincodeType::YCSB);
      }

      crdtWorkload = std::make_unique<crdt::CrdtWorkload>();
      crdtWorkload->init(n);
      ycsbWorkload = std::make_unique<ycsb::CoreWorkload>();
      ycsbWorkload->init(n);
      crdtWorkload->setMeasurements(measurements);
      ycsbWorkload->setMeasurements(measurements);
    }

    bool doTransaction(core::DB* db) const override {
      auto choose = chaincodeChooser->nextValue();
      // auto choose = ChaincodeType::VOTE;
      if (choose == ChaincodeType::VOTE) {
        return crdtWorkload->doTransaction(db);
      } else if (choose == ChaincodeType::YCSB) {
        return ycsbWorkload->doTransaction(db);
        // return true;
      } else {
        return false;
      }
      return true;
    }

    bool doInsert(core::DB* db) const override {
      return false;
    }


  private:
    std::unique_ptr<MixDiscreteGenerator> chaincodeChooser;

    std::unique_ptr<client::crdt::CrdtWorkload> crdtWorkload;
    std::unique_ptr<client::ycsb::CoreWorkload> ycsbWorkload;


  };
}
