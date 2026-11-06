#pragma once

#include "esphome/components/word_clock/word_clock.h"

#include <algorithm>
#include <vector>

namespace esphome::clock_test {

class TestTime : public time::RealTimeClock {
 public:
  void update() override {}
  time_t timestamp_now() override {
    this->reads++;
    return this->epoch;
  }
  time_t epoch{0};
  unsigned reads{0};
};

class TestLight : public light::AddressableLight {
 public:
  explicit TestLight(int count) : pixels_(count), effects_(count) {}
  int32_t size() const override { return this->pixels_.size(); }
  void clear_effect_data() override { std::fill(this->effects_.begin(), this->effects_.end(), 0); }
  light::LightTraits get_traits() override {
    light::LightTraits traits;
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void write_state(light::LightState *state) override { this->shows++; }
  void flush() { this->state_parent_->loop(); }
  unsigned shows{0};
  mutable unsigned pixel_views{0};

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    this->pixel_views++;
    auto &pixel = this->pixels_[index];
    return {&pixel.r, &pixel.g, &pixel.b, &pixel.w, &this->effects_[index], &this->correction_};
  }
  mutable std::vector<Color> pixels_;
  mutable std::vector<uint8_t> effects_;
};

class TestHarness : public Component {
 public:
  TestHarness(word_clock::WordClock *clock, word_clock::WordClock *matrix_only_clock, light::LightState *state,
              light::LightState *matrix_only_state, TestTime *time,
              addressable_light::AddressableLightDisplay *matrix, addressable_light::AddressableLightDisplay *dots,
              addressable_light::AddressableLightDisplay *matrix_only, word_clock::WordClock *failed_clock,
              addressable_light::AddressableLightDisplay *failed_display)
      : clock_(clock), matrix_only_clock_(matrix_only_clock), state_(state), matrix_only_state_(matrix_only_state),
        time_(time), matrix_(matrix), dots_(dots), matrix_only_(matrix_only), failed_clock_(failed_clock),
        failed_display_(failed_display) {}
  float get_setup_priority() const override { return setup_priority::LATE - 100; }
  void setup() override;

 protected:
  word_clock::WordClock *clock_;
  word_clock::WordClock *matrix_only_clock_;
  light::LightState *state_;
  light::LightState *matrix_only_state_;
  TestTime *time_;
  addressable_light::AddressableLightDisplay *matrix_;
  addressable_light::AddressableLightDisplay *dots_;
  addressable_light::AddressableLightDisplay *matrix_only_;
  word_clock::WordClock *failed_clock_;
  addressable_light::AddressableLightDisplay *failed_display_;
};

}  // namespace esphome::clock_test
