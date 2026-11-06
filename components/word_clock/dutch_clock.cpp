#include "dutch_clock.h"

namespace esphome::word_clock {

Phrase select_phrase(uint8_t hour, uint8_t minute) {
  const uint8_t quotient = minute / 5;
  static constexpr std::array<uint32_t, 12> MINUTE_WORDS{{
      word_bit(Word::HOUR),
      word_bit(Word::FIVE_MINUTES) | word_bit(Word::OVER),
      word_bit(Word::TEN_MINUTES) | word_bit(Word::OVER),
      word_bit(Word::QUARTER) | word_bit(Word::OVER_ALTERNATE),
      word_bit(Word::TEN_MINUTES) | word_bit(Word::BEFORE) | word_bit(Word::HALF),
      word_bit(Word::FIVE_MINUTES) | word_bit(Word::BEFORE) | word_bit(Word::HALF),
      word_bit(Word::HALF),
      word_bit(Word::FIVE_MINUTES) | word_bit(Word::OVER) | word_bit(Word::HALF),
      word_bit(Word::TEN_MINUTES) | word_bit(Word::OVER) | word_bit(Word::HALF),
      word_bit(Word::QUARTER) | word_bit(Word::BEFORE_ALTERNATE),
      word_bit(Word::TEN_MINUTES) | word_bit(Word::BEFORE),
      word_bit(Word::FIVE_MINUTES) | word_bit(Word::BEFORE),
  }};
  const uint8_t displayed_hour = (hour + (quotient >= 4 ? 1 : 0)) % 12;
  const Word hour_word =
      displayed_hour == 0 ? Word::TWELVE : static_cast<Word>(static_cast<uint8_t>(Word::ONE) + displayed_hour - 1);
  return {word_bit(Word::HET) | word_bit(Word::IS) | MINUTE_WORDS[quotient] | word_bit(hour_word),
          static_cast<uint8_t>(minute % 5)};
}

}  // namespace esphome::word_clock
