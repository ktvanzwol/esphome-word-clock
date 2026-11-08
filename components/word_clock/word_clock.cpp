#include "word_clock.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::word_clock {

static const char *const TAG = "word_clock";

light::LightTraits WordClock::get_traits() {
  light::LightTraits traits;
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void WordClock::initialize_display_(addressable_light::AddressableLightDisplay *display, light::LightState *backing) {
  // Keep the backing light's uniform-color updates from replacing the display buffer.
  display->get_light()->set_effect_active(true);
  auto call = backing->make_call();
  call.set_state(true);
  call.set_brightness(1.0f);
  call.set_color_brightness(1.0f);
  call.set_rgb(1.0f, 1.0f, 1.0f);
  call.set_transition_length(0);
  call.set_publish(false);
  call.set_save(false);
  call.perform();
}

void WordClock::setup() {
  if (this->matrix_ != nullptr)
    this->matrix_->stop_poller();
  if (this->dots_ != nullptr)
    this->dots_->stop_poller();
  const char *error = nullptr;
  if (this->clock_ == nullptr)
    error = "time source is missing";
  else if (this->state_ == nullptr)
    error = "RGB light state is missing";
  else if (this->matrix_ == nullptr)
    error = "matrix display is missing";
  else if (this->matrix_light_ == nullptr || this->matrix_->get_light() == nullptr)
    error = "matrix backing light is missing";
  else if (this->matrix_->get_width() != 11 || this->matrix_->get_height() != 10)
    error = "matrix display must have logical dimensions 11x10";
  else if (this->matrix_->get_light()->size() < 110)
    error = "matrix backing light has fewer than 110 LEDs";
  else if (this->dots_ != nullptr && this->dots_light_ == nullptr)
    error = "minute-dots backing light is missing";
  else if (this->dots_ != nullptr && this->dots_->get_light() == nullptr)
    error = "minute-dots display has no addressable light";
  else if (this->dots_ != nullptr && (this->dots_->get_width() != 4 || this->dots_->get_height() != 1))
    error = "minute-dots display must have logical dimensions 4x1";
  else if (this->dots_ != nullptr && this->dots_->get_light()->size() < 4)
    error = "minute-dots backing light has fewer than 4 LEDs";
  if (error != nullptr) {
    ESP_LOGE(TAG, "Invalid word clock configuration: %s", error);
    this->mark_failed();
    return;
  }
  this->initialize_display_(this->matrix_, this->matrix_light_);
  if (this->dots_ != nullptr)
    this->initialize_display_(this->dots_, this->dots_light_);
  this->ready_ = true;
  this->write_state(this->state_);
}

void WordClock::dump_config() {
  ESP_LOGCONFIG(TAG, "Dutch Word Clock:");
  ESP_LOGCONFIG(TAG, "  Matrix: 11x10");
  ESP_LOGCONFIG(TAG, "  Minute dots: %s", this->dots_ != nullptr ? "4" : "disabled");
}

void WordClock::write_state(light::LightState *state) {
  float red, green, blue;
  state->current_values_as_rgb(&red, &green, &blue);
  this->color_ = Color(light::to_uint8_scale(red), light::to_uint8_scale(green), light::to_uint8_scale(blue));
  if (this->ready_)
    this->render_(this->clock_->now());
}

void WordClock::update() {
  if (this->ready_)
    this->render_(this->clock_->now());
}

void WordClock::render_(const ESPTime &now) {
  const bool valid = now.is_valid();
  if (!this->time_valid_.has_value() || *this->time_valid_ != valid) {
    this->time_valid_ = valid;
    if (valid) {
      ESP_LOGI(TAG, "Time is valid");
      this->status_clear_warning();
    } else {
      ESP_LOGW(TAG, "Time unavailable; clock is blank");
      this->status_set_warning();
    }
  }
  Color foreground = valid ? this->color_ : Color(0, 0, 0);
  Phrase phrase{};
  if (foreground != Color(0, 0, 0)) {
    phrase = select_phrase(now.hour, now.minute);
    if (this->dots_ == nullptr)
      phrase.minute_dots = 0;
  }
  if (this->rendered_ && phrase == this->rendered_phrase_ && foreground == this->rendered_color_)
    return;

  this->matrix_->clear();
  for (size_t i = 0; i < WORD_SPANS.size(); i++) {
    if ((phrase.words & (uint32_t{1} << i)) != 0) {
      const auto &span = WORD_SPANS[i];
      this->matrix_->line(span.x, span.y, span.x + span.length - 1, span.y, foreground);
    }
  }
  this->matrix_->display();
  if (this->dots_ != nullptr) {
    this->dots_->clear();
    if (phrase.minute_dots != 0)
      this->dots_->line(0, 0, phrase.minute_dots - 1, 0, foreground);
    this->dots_->display();
  }
  this->rendered_phrase_ = phrase;
  this->rendered_color_ = foreground;
  this->rendered_ = true;
}

void ColorCycleEffect::apply() {
  const uint32_t tick = millis();
  if (tick - this->last_update_ < 1000)
    return;
  this->last_update_ = tick;
  const auto now = this->clock_->now();
  if (!now.is_valid()) {
    if (!this->invalid_time_)
      ESP_LOGW(TAG, "Color cycle paused: time unavailable");
    this->invalid_time_ = true;
    this->last_phase_ = -1;
    return;
  }
  this->invalid_time_ = false;
  int phase;
  float hue;
  switch (this->period_) {
    case CyclePeriod::DAILY:
      phase = now.hour * 60 + now.minute;
      hue = 1.0f - phase / 1440.0f;
      break;
    case CyclePeriod::HOURLY:
      phase = now.minute * 60 + now.second;
      hue = 1.0f - phase / 3600.0f;
      break;
    case CyclePeriod::MINUTE:
      phase = now.second;
      hue = 1.0f - phase / 60.0f;
      break;
    default:
      ESP_LOGE(TAG, "Invalid color cycle period");
      return;
  }
  if (phase == this->last_phase_)
    return;
  this->last_phase_ = phase;

  const float sector = hue * 6.0f;
  const int index = static_cast<int>(sector) % 6;
  const float fraction = sector - std::floor(sector);
  float red = 0, green = 0, blue = 0;
  switch (index) {
    case 0: red = 1; green = fraction; break;
    case 1: red = 1 - fraction; green = 1; break;
    case 2: green = 1; blue = fraction; break;
    case 3: green = 1 - fraction; blue = 1; break;
    case 4: red = fraction; blue = 1; break;
    case 5: red = 1; blue = 1 - fraction; break;
  }
  auto call = this->state_->make_call();
  call.set_rgb(red, green, blue);
  call.set_transition_length(this->period_ == CyclePeriod::MINUTE ? 1000 : 0);
  call.set_publish(false);
  call.set_save(false);
  call.perform();
}

}  // namespace esphome::word_clock
