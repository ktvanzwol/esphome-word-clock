#include "esphome.h"

// Read RGB values from global variables.
// These are written via output templates assosiated with a RGB Light.
// This allows Color and Brightness controll from Home Assistant.
Color getForegroundColor() 
{
  uint8_t red = id(red_value) * 255.0;
  uint8_t green = id(green_value) * 255.0;
  uint8_t blue = id(blue_value) * 255.0;

  // create HSB color
  HsbColor color = HsbColor(RgbColor(red, green, blue));

  // scale brightness, need at least 25% to overcome difuser
  if (color.B > 0.0f) 
    color.B = color.B * 0.75f + 0.25f;
  else
    color.B = 0.0f;

  // convert to RGB format.
  RgbColor c = RgbColor(color);

  // return ESPHome color
  return Color(c.R, c.G, c.B);
}

// Update the dots representing minutes to add to the time
// represented by the text.
// This function is called by the addressable_light display.lambda
void UpdateMinuteDots(display::Display & it) 
{
  // Get Color
  Color fc = getForegroundColor();
  
  // get the remainer of 5 minutes
  int remainder = id(sntp_time).now().minute % 5;

  // if remainder is none zero draw a line with corresponding # of pixels
  if (remainder > 0)
    it.line(0, 0, remainder - 1, 0, fc);
}

/* Pixel Mapper function for the Text Display
  LED Address vs x, y
        X  0   1   2   3   4   5   6   7   8   9  10
  Y RY ----------------------------------------------
  0  9 | 109 108 107 106 105 104 103 102 101 100  99
  1  8 |  88                  >                   98
  2  7 |  87                  <                   77
  3  6 |  66                  >                   76
  4  5 |  65                  <                   55
  5  4 |  44                  >                   54
  6  3 |  43                  <                   33
  7  2 |  22  23  24  25  26  27  28  29  30  31  32
  8  1 |  21  20  19  18  17  16  15  14  13  12  11
  9  0 |   0   1   2   3   4   5   6   7   8   9  10
*/
int32_t MapLetterPixel(int32_t x, int32_t y)
{
  // Reverse Y
  int32_t ry = 9 - y;

  // Handle alternating rows
  if (ry % 2 == 0) {
    return (ry * 11) + x;
  }
  else {
    return (ry * 11) + (10 - x);
  }
}

/*  Update the text representing the current time.
    This function is called by the addressable_light display.lambda

    Clock face layout

      X 0  1  2  3  4  5  6  7  8  9 10
    Y -----------------------------------
    0 | H  E  T  *  I  S  *  V  I  J  F |
    1 | T  I  E  N  *  *  *  V  O  O  R |
    2 | O  V  E  R  *  *  K  W  A  R  T |
    3 | H  A  L  F  *  *  *  O  V  E  R |
    4 | V  O  O  R  *  *  *  E  E  N  * |
    5 | T  W  E  E  *  *  *  D  R  I  E |
    6 | V  I  E  R  V  I  J  F  Z  E  S |
    7 | Z  E  V  E  N  *  N  E  G  E  N |
    8 | A  C  H  T  T  I  E  N  E  L  F |
    9 | T  W  A  A  L  F  *  *  U  U  R |
      -----------------------------------
    0 |             0 0 0 0             |
      -----------------------------------
                    0 1 2 3

    Text flow for one hour

    |    |      | Text to display                              |
    | Qu | Time | [1]    | [2]  | [3]       | [4]  | [5] | [6] |
    |-----------------------------------------------------------
    |  4 | 5:20 | Het is | tien | voor      | hafl | zes |     |
    |  5 | 5:25 | Het is | vijf | voor      | hafl | zes |     |
    |  6 | 5:30 | Het is |      |           | hafl | zes |     |
    |  7 | 5:35 | Het is | vijf | over      | hafl | zes |     |
    |  8 | 5:40 | Het is | tien | over      | hafl | zes |     |
    |  9 | 5:45 | Het is | kwart|      voor |      | zes |     |
    | 10 | 5:50 | Het is | tien | voor      |      | zes |     |
    | 11 | 5:55 | Het is | vijf | voor      |      | zes |     |
    |  0 | 6:00 | Het is |      |           |      | zes | uur |
    |  1 | 5:05 | Het is | vijf | over      |      | zes |     |
    |  2 | 5:10 | Het is | tien | over      |      | zes |     |
    |  3 | 5:15 | Het is | kwart|      over |      | zes |     |

    Quotient (Qu) is the interget division of minutes by 5.
    This drives which text is displayed.
*/ 
void UpdateText(display::Display & it) 
{
  // Get Color
  Color fc = getForegroundColor();

  // Get Time
  auto t = id(sntp_time).now();
  int quotient = t.minute / 5;
  int hours = t.hour;

  // shift hours for before | after the hour
  if (quotient >= 4)
    hours++;
  
  // 24 to 12 hours
  hours = hours % 12;

  // [1] draw static text 'Het is'
  it.line(0,0, 2,0, fc); // Het
  it.line(4,0, 5,0, fc); // is

  // [2] Update text for 5, 10 and 15min intervals except half hours    
  switch (quotient)
  {
    // Five minute multiples
    case 1: case 5: case 7: case 11: it.line(7, 0, 10, 0, fc); break;
    // Ten minute multiples
    case 2: case 4: case 8: case 10: it.line(0, 1, 3, 1, fc); break;
    // Quarters
    case 9: case 3: it.line(6, 2, 10, 2, fc); break;
    default: break;
  }

  // [3] Set before and after
  switch (quotient)
  {
    // After
    case 1: case 2: case 7: case 8: it.line(0, 2, 3, 2, fc); break;
    // After alternate.
    case 3: it.line(7, 3, 10, 3, fc); break;
    // Before
    case 4: case 5: case 10: case 11: it.line(7, 1, 10, 1, fc); break;
    // Before alternate
    case 9: it.line(0, 4, 3, 4, fc); break;
    default: break;
  }

  // [4] update text half hours
  switch (quotient)
  {
    case 4: case 5: case 6: case 7: case 8: it.line(0, 3, 3, 3, fc); break;
    default: break;
  }

  // [5] Update hours
  switch (hours)
  {
    case 1:  it.line(7, 4, 9,  4, fc); break;
    case 2:  it.line(0, 5, 3,  5, fc); break;
    case 3:  it.line(7, 5, 10, 5, fc); break;
    case 4:  it.line(0, 6, 3,  6, fc); break;
    case 5:  it.line(4, 6, 7,  6, fc); break;
    case 6:  it.line(8, 6, 10, 6, fc); break;
    case 7:  it.line(0, 7, 4,  7, fc); break;
    case 8:  it.line(0, 8, 3,  8, fc); break;
    case 9:  it.line(6, 7, 10, 7, fc); break;
    case 10: it.line(4, 8, 7,  8, fc); break;
    case 11: it.line(8, 8, 10, 8, fc); break;
    case 0:  it.line(0, 9, 5,  9, fc); break; /* 12 */
    default: break;
  }

  // [6] The whole hour  
  if (quotient == 0) 
    it.line(8, 9, 10,  9, fc);
}

void EffectMinuteColorCycle() {
    HsbColor color;
    auto t = id(sntp_time).now();

    // HUE Color reference: https://en.wikipedia.org/wiki/Hue 

    // Set Hue to current time scaled one hue rotation per minute.
    color.H = t.second / 60.0f;
    color.B = 1.0f;
    color.S = 1.0f;

    // Rotate color counter clock wise:
    // Red -> Magenta -> Blue -> Cyan -> Green -> Yellow -> Red
    color.H = 1.0f - color.H;

    // convert to RGB
    RgbColor new_color = RgbColor(color);

    // Update light
    auto call = id(word_clock_light).turn_on();
    call.set_transition_length(1000);
    call.set_red(new_color.R / 255.0f);
    call.set_green(new_color.G / 255.0f);
    call.set_blue(new_color.B / 255.0f);
    call.perform();
}


void EffectHourlyColorCycle() {
    HsbColor color;
    auto t = id(sntp_time).now();

    // HUE Color reference: https://en.wikipedia.org/wiki/Hue 

    // Set Hue to current time scaled one hue rotation per hour.
    color.H = (t.minute * 60.0f + t.second) / (60.0f * 60.0f);
    color.B = 1.0f;
    color.S = 1.0f;

    // Rotate color counter clock wise:
    // Red -> Magenta -> Blue -> Cyan -> Green -> Yellow -> Red
    color.H = 1.0f - color.H;

    // convert to RGB
    RgbColor new_color = RgbColor(color);

    // Update light
    auto call = id(word_clock_light).turn_on();
    call.set_transition_length(0);
    call.set_red(new_color.R / 255.0f);
    call.set_green(new_color.G / 255.0f);
    call.set_blue(new_color.B / 255.0f);
    call.perform();
}

void EffectDailyColorCycle() {
    HsbColor color;
    auto t = id(sntp_time).now();

    // HUE Color reference: https://en.wikipedia.org/wiki/Hue 

    // Set Hue to current time scaled one hue rotation per day.
    color.H = (t.hour * 60.0f + t.minute) / (24.0f * 60.0f);
    color.B = 1.0f;
    color.S = 1.0f;

    // Rotate color counter clock wise over the day:
    // Red -> Magenta -> Blue -> Cyan -> Green -> Yellow -> Red
    // Also makes Blue match to 8 in the moring and Yellow to 8 in the evening.
    color.H = 1.0f - color.H;

    // convert to RGB
    RgbColor new_color = RgbColor(color);

    // Update light
    auto call = id(word_clock_light).turn_on();
    call.set_transition_length(0);
    call.set_red(new_color.R / 255.0f);
    call.set_green(new_color.G / 255.0f);
    call.set_blue(new_color.B / 255.0f);
    call.perform();
}