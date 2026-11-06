#pragma once

#include <array>
#include <cstdint>

namespace esphome::word_clock {

/// Logical face words; ordinal values index WORD_SPANS and identify bits in Phrase::words.
enum class Word : uint8_t {
  HET,
  IS,
  FIVE_MINUTES,
  TEN_MINUTES,
  QUARTER,
  OVER,
  BEFORE,
  OVER_ALTERNATE,
  BEFORE_ALTERNATE,
  HALF,
  ONE,
  TWO,
  THREE,
  FOUR,
  FIVE,
  SIX,
  SEVEN,
  EIGHT,
  NINE,
  TEN,
  ELEVEN,
  TWELVE,
  HOUR,
};

/// A horizontal word in unrotated face coordinates, not hardware LED indices.
struct WordSpan {
  uint8_t x;
  uint8_t y;
  uint8_t length;
};

inline constexpr std::array<WordSpan, 23> WORD_SPANS{{
    {0, 0, 3}, {4, 0, 2}, {7, 0, 4}, {0, 1, 4}, {6, 2, 5}, {0, 2, 4},
    {7, 1, 4}, {7, 3, 4}, {0, 4, 4}, {0, 3, 4}, {7, 4, 3}, {0, 5, 4},
    {7, 5, 4}, {0, 6, 4}, {4, 6, 4}, {8, 6, 3}, {0, 7, 5}, {0, 8, 4},
    {6, 7, 5}, {4, 8, 4}, {8, 8, 3}, {0, 9, 6}, {8, 9, 3},
}};

constexpr uint32_t word_bit(Word word) { return uint32_t{1} << static_cast<uint8_t>(word); }

/// Selected face words and the minute remainder (0..4), independent of rendering.
struct Phrase {
  uint32_t words{0};
  uint8_t minute_dots{0};

  bool operator==(const Phrase &other) const {
    return this->words == other.words && this->minute_dots == other.minute_dots;
  }
};

/**
 * Select the Dutch phrase for a valid local hour (0..23) and minute (0..59).
 *
 * Minutes are rounded down to a five-minute block. From minute 20 the spoken
 * hour advances, wrapping to twelve; minute_dots carries the unspoken remainder.
 * The caller must validate time before calling; this pure function reads no clock.
 */
Phrase select_phrase(uint8_t hour, uint8_t minute);

}  // namespace esphome::word_clock
