//
// Created by miku on 25-4-27.
//

#ifndef NORMAL_UNINT64_H
#  define NORMAL_UNINT64_H
#  include <memory>

#  include "client/core/generator/generator.h"

#endif //NORMAL_UNINT64_H


namespace client::utils {
  class NornamlUINT64 : public client::core::NumberGenerator {
  public:
    static auto NewNormalUINT64(uint64_t min, uint64_t max) {
      return std::make_unique<NornamlUINT64>(min, max);
    }
    explicit NornamlUINT64(uint64_t min, uint64_t max, double mean = 20, double stddev = 2, uint64_t seed = std::random_device{}()) {
      mean_ = mean;
      min_ = min;
      max_ = max;
      distribution = std::normal_distribution<double>(mean, stddev);
      generator = std::mt19937(seed);
      max_val = mean + 3 * stddev;
      min_val = mean - 3 * stddev;
      val_range = max_val - min_val;
      range = static_cast<double>(max) - static_cast<double>(min);
    }

    uint64_t nextValue() override {
      double normal_sample = distribution(generator);

      normal_sample = std::max(min_val, std::min(max_val, normal_sample));

      double normalized = (normal_sample - min_val) / val_range;

      return min_ + static_cast<uint64_t>(normalized * range + 0.5);
    }

    double mean() override {
      return mean_;
    }

  private:
    std::mt19937 generator;
    std::normal_distribution<double> distribution;
    double mean_;
    uint64_t min_;
    uint64_t max_;
    double max_val;
    double min_val;
    double val_range;
    double range;
  };
}
