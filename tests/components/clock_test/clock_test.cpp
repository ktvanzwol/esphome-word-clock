#include "clock_test.h"
#include "esphome/core/application.h"

#include <cstdlib>
#include <iostream>

namespace esphome::clock_test {

static void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

static unsigned lit_pixels(light::AddressableLight *output) {
  unsigned count = 0;
  for (int i = 0; i < output->size(); i++) {
    const auto pixel = (*output)[i].get();
    if (pixel.r || pixel.g || pixel.b)
      count++;
  }
  return count;
}

void TestHarness::setup() {
  require(!this->clock_->is_failed() && !this->matrix_only_clock_->is_failed(), "clock setup succeeded");
  require(this->failed_clock_->is_failed(), "runtime capacity mismatch fails explicitly");
  auto *failed_output = static_cast<TestLight *>(this->failed_display_->get_light());
  failed_output->flush();
  const unsigned failed_views = failed_output->pixel_views;
  delay(30);
  App.scheduler.call(millis());
  failed_output->flush();
  require(failed_output->pixel_views == failed_views, "invalid-capacity display polling is stopped safely");
  require(lit_pixels(this->matrix_->get_light()) == 0 && lit_pixels(this->dots_->get_light()) == 0,
          "invalid time starts black");
  this->state_->turn_on().set_rgb(1, 0, 0).set_brightness(1).set_transition_length(0).perform();
  this->clock_->write_state(this->state_);
  require(lit_pixels(this->matrix_->get_light()) == 0, "turning on with invalid time stays black");

  this->time_->epoch = 1704067200;  // 2024-01-01 00:00 UTC.
  this->time_->reads = 0;
  this->clock_->update();
  require(this->time_->reads == 1, "one time read for words and dots");
  require(lit_pixels(this->matrix_->get_light()) == 14, "midnight displays HET IS TWAALF UUR");
  require(lit_pixels(this->dots_->get_light()) == 0, "whole hour has no dots");
  auto *matrix_output = static_cast<TestLight *>(this->matrix_->get_light());
  auto *dots_output = static_cast<TestLight *>(this->dots_->get_light());
  matrix_output->flush();
  dots_output->flush();
  const unsigned matrix_shows = matrix_output->shows;
  const unsigned dots_shows = dots_output->shows;
  for (int second = 1; second < 60; second++) {
    this->time_->epoch = 1704067200 + second;
    this->clock_->update();
    matrix_output->flush();
    dots_output->flush();
  }
  require(matrix_output->shows == matrix_shows && dots_output->shows == dots_shows,
          "unchanged seconds do not schedule redundant hardware frames");
  for (int remainder = 0; remainder <= 4; remainder++) {
    this->time_->epoch = 1704067200 + remainder * 60;
    this->clock_->update();
    require(lit_pixels(this->dots_->get_light()) == static_cast<unsigned>(remainder), "minute dot count");
    for (int i = 0; i < 4; i++)
      require(((*this->dots_->get_light())[i].get_red() != 0) == (i >= 4 - remainder),
              "dots use ESPHome rotation, without another reversal");
  }
  this->time_->epoch = 1704067500;
  this->clock_->update();
  require(lit_pixels(this->dots_->get_light()) == 0, "old dots clear at five-minute boundary");
  require((*this->matrix_->get_light())[9 * 11 + 8].get_red() == 0, "old UUR pixels clear");

  this->state_->make_call().set_rgb(0, 1, 0).set_brightness(0.01f).set_transition_length(0).perform();
  this->clock_->write_state(this->state_);
  require((*this->matrix_->get_light())[0].get_red() == 0, "color change clears old red");
  require((*this->matrix_->get_light())[0].get_green() == 3, "one percent brightness has no floor or boost");
  this->state_->make_call().set_brightness(1).set_transition_length(0).perform();
  this->clock_->write_state(this->state_);
  require((*this->matrix_->get_light())[0].get_green() == 255, "full brightness reaches full scale");

  this->state_->turn_off().set_transition_length(200).perform();
  delay(100);
  this->state_->loop();
  const auto middle = (*this->matrix_->get_light())[0].get_green();
  require(middle > 0 && middle < 255, "native off transition has an intermediate frame");
  delay(130);
  this->state_->loop();
  require(lit_pixels(this->matrix_->get_light()) == 0 && lit_pixels(this->dots_->get_light()) == 0,
          "completed off transition is black");

  this->state_->turn_on().set_rgb(1, 0, 0).set_transition_length(0).perform();
  this->clock_->write_state(this->state_);
  this->time_->epoch = 0;
  this->clock_->update();
  require(lit_pixels(this->matrix_->get_light()) == 0, "lost time clears stale words");
  this->time_->epoch = 1704067200;
  this->clock_->update();
  require(lit_pixels(this->matrix_->get_light()) == 14, "valid time recovers without another light command");

  this->matrix_only_state_->turn_on().set_rgb(0, 0, 1).set_transition_length(0).perform();
  this->matrix_only_clock_->write_state(this->matrix_only_state_);
  require(lit_pixels(this->matrix_only_->get_light()) == 14, "matrix-only runtime renders correctly");
  require((*this->matrix_only_->get_light())[10].get_blue() == 255 &&
              (*this->matrix_only_->get_light())[0].get_blue() == 0,
          "ESPHome's display mapper is honored");
  this->matrix_only_state_->make_call().set_brightness(0.5f).set_transition_length(0).perform();
  this->matrix_only_clock_->write_state(this->matrix_only_state_);
  require((*this->matrix_only_->get_light())[10].get_blue() == 37, "native gamma 2.8 is applied exactly once");

  this->state_->make_call().set_effect("Daily").set_brightness(0.2f).perform();
  this->time_->epoch = 1704067200 + 8 * 3600;
  this->state_->loop();
  require(this->state_->current_values.get_blue() > 0.99f && this->state_->current_values.get_red() < 0.01f,
          "daily cycle is blue at 08:00");
  require(this->state_->remote_values.get_brightness() == 0.2f, "cycle preserves brightness");
  require(this->state_->remote_values.get_red() > 0.99f, "cycle does not publish each RGB update");
  this->time_->reads = 0;
  for (int i = 0; i < 10; i++)
    this->state_->loop();
  require(this->time_->reads == 0, "effect time reads are throttled, not repeated every loop");
  this->time_->epoch = 1704067200 + 20 * 3600;
  delay(1001);
  this->state_->loop();
  require(this->state_->current_values.get_red() > 0.99f && this->state_->current_values.get_green() > 0.99f,
          "daily cycle is yellow at 20:00");
  this->time_->epoch = 1704067200 + 20 * 60;
  this->state_->make_call().set_effect("Hourly").perform();
  this->state_->loop();
  require(this->state_->current_values.get_blue() > 0.99f && this->state_->current_values.get_red() < 0.01f,
          "hourly cycle has the original hue direction");
  this->time_->epoch = 1704067200 + 20;
  this->state_->make_call().set_effect("Minute").perform();
  this->state_->loop();
  delay(500);
  this->state_->loop();
  const float transitioning_red = this->state_->current_values.get_red();
  require(transitioning_red > 0 && transitioning_red < 1, "minute cycle uses a native one-second transition");
  delay(520);
  this->state_->loop();
  require(this->state_->current_values.get_blue() > 0.99f && this->state_->current_values.get_red() < 0.01f,
          "minute cycle reaches its time-derived hue");
  this->state_->turn_off().set_transition_length(0).perform();
  this->time_->epoch += 60;
  this->state_->loop();
  require(!this->state_->remote_values.is_on(), "effects cannot force an off light on");
  require(lit_pixels(this->matrix_->get_light()) == 0, "off with effect is black");

  std::cout << "PASS: native ESPHome rendering, invalid time, rotation, brightness, transitions, effects, optional dots\n";
  std::exit(0);
}

}  // namespace esphome::clock_test
