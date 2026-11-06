#pragma once

#include "dutch_clock.h"
#include "esphome/components/addressable_light/addressable_light_display.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome::word_clock {

/**
 * Native RGB light output rendering through exclusively owned ESPHome displays.
 *
 * ESPHome owns every referenced object and performs rotation, mapping, and LED
 * transmission. This class owns only phrase/color caches, not a hardware buffer.
 * Polling detects time changes; write_state receives native transition steps.
 */
class WordClock : public PollingComponent, public light::LightOutput {
 public:
  WordClock() : PollingComponent(1000) {}
  void set_time(time::RealTimeClock *clock) { this->clock_ = clock; }
  void set_matrix(addressable_light::AddressableLightDisplay *display, light::LightState *backing) {
    this->matrix_ = display;
    this->matrix_light_ = backing;
  }
  void set_dots(addressable_light::AddressableLightDisplay *display, light::LightState *backing) {
    this->dots_ = display;
    this->dots_light_ = backing;
  }
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void setup() override;
  void dump_config() override;
  // Backing lights and display buffers must be initialized before clock rendering.
  float get_setup_priority() const override { return setup_priority::LATE; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;
  void update() override;

 protected:
  /// Reuse one time snapshot for words/dots; invalid time becomes a cached black frame.
  void render_(const ESPTime &now);
  /// Enable full-scale backing output without saving/publishing or replacing display pixels.
  void initialize_display_(addressable_light::AddressableLightDisplay *display, light::LightState *backing);

  time::RealTimeClock *clock_{nullptr};
  light::LightState *state_{nullptr};
  addressable_light::AddressableLightDisplay *matrix_{nullptr};
  addressable_light::AddressableLightDisplay *dots_{nullptr};
  light::LightState *matrix_light_{nullptr};
  light::LightState *dots_light_{nullptr};
  Color color_{0, 0, 0};
  Color rendered_color_{0, 0, 0};
  Phrase rendered_phrase_{};
  optional<bool> time_valid_;
  bool ready_{false};
  bool rendered_{false};
};

enum class CyclePeriod : uint8_t { DAILY, HOURLY, MINUTE };

/**
 * Counter-clockwise, local-time-derived hue cycle using native RGB light calls.
 *
 * Application is throttled to one second and skips unchanged phases. Color-only
 * calls preserve on/off and brightness, do not publish/save, and never run from
 * rendering. Invalid time pauses the cycle; the clock independently renders black.
 */
class ColorCycleEffect : public light::LightEffect {
 public:
  ColorCycleEffect(const char *name, time::RealTimeClock *clock, CyclePeriod period)
      : LightEffect(name), clock_(clock), period_(period) {}
  void start() override {
    this->last_phase_ = -1;
    this->last_update_ = millis() - 1000;
  }
  void apply() override;

 protected:
  time::RealTimeClock *clock_;
  CyclePeriod period_;
  int last_phase_{-1};
  uint32_t last_update_{0};
  bool invalid_time_{false};
};

}  // namespace esphome::word_clock
