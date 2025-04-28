//
// Created by miku on 25-4-28.
//

#pragma once

# include "client/core/default_property.h"


namespace client::mix {
  enum class ChaincodeType {
    VOTE,
    YCSB,
  };


  class MixProperties : public core::BaseProperties<MixProperties> {

  public:
    constexpr static const auto PROPERTY_NAME = "mix_setting";

    constexpr static const auto VOTE_PROPORTION = "vote_proportion";
    constexpr static const auto YCSB_PROPORTION = "ycsb_proportion";


    inline auto getVoteProportion() const {
      return n[VOTE_PROPORTION].as<double>(0.5);
    }

    inline auto getYCSBProportion() const {
      return n[YCSB_PROPORTION].as<double>(0.5);
    }

    explicit MixProperties(const YAML::Node& node) : core::BaseProperties<MixProperties>(node) {}

  };
}
