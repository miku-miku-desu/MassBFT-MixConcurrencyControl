//
// Created by miku on 25-4-28.
//

#pragma once
#include <memory>

#include "mix_workload.h"

namespace client::mix {
  struct MixFactory {
    static std::shared_ptr<core::Workload> CreateWorkload(const util::Properties &) {
      return std::make_shared<MixWorkload>();
    }

    static std::unique_ptr<MixProperties> CreateProperty(const util::Properties& n) {
      return MixProperties::NewFromProperty(n);
    }
  };

  using MixEngine = core::DefaultEngine<MixFactory, MixProperties>;
}
