#include "../components/word_clock/dutch_clock.h"

#include <cassert>
#include <cctype>
#include <iostream>
#include <string>
#include <array>

using namespace esphome::word_clock;

int main() {
  const std::array<std::string, 12> minutes{{
      "uur", "vijf over", "tien over", "kwart over2", "tien voor half", "vijf voor half",
      "half", "vijf over half", "tien over half", "kwart voor2", "tien voor", "vijf voor",
  }};
  const std::array<std::string, 12> hours{{
      "twaalf", "een", "twee", "drie", "vier", "vijfuur", "zes", "zeven", "acht", "negen", "tienuur", "elf",
  }};
  const std::array<std::string, 23> names{{
      "het", "is", "vijf", "tien", "kwart", "over", "voor", "over2", "voor2", "half",
      "een", "twee", "drie", "vier", "vijfuur", "zes", "zeven", "acht", "negen", "tienuur", "elf", "twaalf", "uur",
  }};
  for (uint8_t hour = 0; hour < 24; hour++) {
    for (uint8_t minute = 0; minute < 60; minute++) {
      const Phrase phrase = select_phrase(hour, minute);
      const std::string expected = " het is " + minutes[minute / 5] + " " +
                                   hours[(hour + (minute >= 20 ? 1 : 0)) % 12] + " ";
      uint32_t expected_words = 0;
      for (size_t i = 0; i < names.size(); i++) {
        if (expected.find(" " + names[i] + " ") != std::string::npos)
          expected_words |= uint32_t{1} << i;
      }
      assert(phrase.words == expected_words);
      assert(phrase.minute_dots == minute % 5);
    }
  }
  const std::array<std::string, 10> face{{
      "HET IS VIJF", "TIEN   VOOR", "OVER  KWART", "HALF   OVER", "VOOR   EEN ",
      "TWEE   DRIE", "VIERVIJFZES", "ZEVEN NEGEN", "ACHTTIENELF", "TWAALF  UUR",
  }};
  for (size_t i = 0; i < WORD_SPANS.size(); i++) {
    const auto &span = WORD_SPANS[i];
    assert(span.x + span.length <= 11 && span.y < 10);
    std::string expected = names[i];
    if (expected == "vijfuur") expected = "vijf";
    if (expected == "tienuur") expected = "tien";
    if (expected == "over2") expected = "over";
    if (expected == "voor2") expected = "voor";
    for (char &character : expected)
      character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    assert(face[span.y].substr(span.x, span.length) == expected);
  }
  std::cout << "PASS: all 1440 daily phrases, minute remainders, and Dutch face spans\n";
}
