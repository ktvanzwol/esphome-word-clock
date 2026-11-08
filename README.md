# ESPHome Dutch word clock

A local external component in `components/word_clock/`, exposing one native RGB light to Home Assistant. It renders the original 11x10 Dutch face, advances the spoken hour from minute 20, and optionally shows the remaining zero to four minutes as dots.

## Configuration

See [the example](example/word-clock.yaml). It uses WS2812 RMT strips on GPIO26 (110 matrix LEDs) and GPIO25 (4 dots), with ESPHome's recommended ESP-IDF framework. Put your existing `secrets.yaml` alongside the example; do not commit it.

The example fetches `components/word_clock/` from this repository's `main` branch. Publish that component directory to the configured branch before validating or compiling the example; the repository's current published `main` branch does not yet contain it.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/ktvanzwol/esphome-word-clock
      ref: main
    components: [word_clock]

light:
  - platform: word_clock
    name: "Word Clock"
    time_id: sntp_time
    matrix_display_id: letters
    dots_display_id: dots
```

- `time_id`: required ESPHome real-time clock.
- `matrix_display_id`: required `addressable_light` display with logical dimensions 11x10.
- `dots_display_id`: optional `addressable_light` display with logical dimensions 4x1. For matrix-only operation, omit this field and the unused dots display/light.
- Standard RGB light options, including gamma, restoration, transitions, and native effects, are supported.
- `word_clock_color_cycle` is a native RGB effect with `time_id` and `period: daily`, `hourly`, or `minute`. The example retains the original effect names and counter-clockwise hue progression. The minute cycle transitions over one second; the other cycles change without a transition. Effects preserve brightness and do not save/publish each color update.

The component draws only logical coordinates. Set wiring-specific `pixel_mapper` and rotation on ESPHome's displays, not the clock. The example preserves the original serpentine/bottom-up matrix mapping and dots rotation of 180 degrees; there is no additional dot reversal.

Referenced displays and their backing lights belong exclusively to the clock: no display lambda/pages, competing displays, backing effects, or separately exposed hardware lights. Set backing lights to `internal: true` and `gamma_correct: 1.0`. The native clock light applies gamma and brightness once, without a brightness floor or boost. At setup the component stops display polling and initializes the backing lights at full scale without saving their state, even if their restore mode is `ALWAYS_OFF`. Native clock state still determines whether pixels are lit.

Time is checked once per second, but unchanged frames are not redrawn. Native light updates and transition steps render promptly. Invalid time clears both displays and sets a component warning; completed off transitions render black.

## Implementation guide

| Source | Responsibility |
| --- | --- |
| [light.py](components/word_clock/light.py) | RGB schema, native effect registration, cross-component validation, and code generation. |
| [dutch_clock.h](components/word_clock/dutch_clock.h) / [dutch_clock.cpp](components/word_clock/dutch_clock.cpp) | Pure phrase selection and logical word spans; no ESPHome state, driver, or hardware mapping. |
| [word_clock.h](components/word_clock/word_clock.h) / [word_clock.cpp](components/word_clock/word_clock.cpp) | Native light output, display lifecycle, frame caching, and time-derived effects. |

### Rendering and ownership

ESPHome owns the referenced time source, light states, and displays. The component stores non-owning pointers and initializes at late setup priority, after the hardware and display buffers exist. It stops the displays' pollers before checking runtime capacity, so a rejected configuration cannot continue flushing an oversized display buffer.

Backing lights remain full-scale internal transport outputs. Their addressable `effect_active` flag prevents normal uniform-color updates from replacing clock pixels; this is not a user-selectable backing effect. Do not independently control these lights or resume the display pollers from automations.

`write_state()` converts the native clock light's current RGB values, including interpolated brightness/on-off and gamma, to pixel colors. The one-second `update()` checks time independently of light changes. Both pass one time snapshot to rendering, which compares the visible phrase, optional dot count, and quantized color against the last frame. A changed frame clears stale pixels, draws logical word spans/dots, and calls ESPHome's display flush. Off or invalid-time frames contain no selected words.

### Dutch face and effects

`Word` ordinal values must stay aligned with `WORD_SPANS` and the phrase bitmask. The two physical occurrences of "over" and "voor" have separate word identifiers; minute words and hour words also use distinct identifiers. If changing the face, update the spans and the face assertions together. `select_phrase()` requires an already validated local hour/minute and has no invalid-time fallback.

Color cycles are separate native `LightEffect` objects, not rendering callbacks. Daily hue uses local hour/minute, hourly hue uses minute/second, and minute hue uses second. Each makes color-only, non-publishing/non-saving light calls, preserving user brightness and state. The minute effect requests a native one-second transition. Invalid time pauses the effect and resets its phase cache so valid time can resume it.

### Test structure

- [dutch_clock_test.cpp](tests/dutch_clock_test.cpp) checks all daily phrases, remainders, and the literal face layout.
- [host.yaml](tests/host.yaml) and [clock_test](tests/components/clock_test/) exercise the real ESPHome light/display APIs with an in-memory addressable output and controlled time. They cover native transitions, mapping/rotation, gamma, all cycle periods, optional dots, unchanged frames, and runtime capacity failure without hardware.
- [validate.py](tests/validate.py) stages disposable configurations, checks negative schemas, and optionally builds/runs native tests and both ESP32 firmware variants. It validates [word-clock.yaml](tests/word-clock.yaml), a self-contained copy of the example that uses local components and dummy values, so validation does not depend on the user-facing example's placeholders.

## Validation without flashing

Use the ESPHome CLI in its Docker image through the WSL Container CLI (`wslc`), not the dashboard. The inspected release is ESPHome 2026.10.2.

```powershell
wslc run --rm ghcr.io/esphome/esphome:2026.10.2 version
wslc run --rm --entrypoint python --mount 'type=bind,source=C:\Git\esphome-word-clock,target=/repo,readonly' ghcr.io/esphome/esphome:2026.10.2 /repo/tests/validate.py --work /tmp/word-clock-validation --compile
```

Adjust the Windows host path to your checkout. The runner stages sources outside the checkout, tests all 1,440 daily phrases and negative schemas, runs native ESPHome runtime assertions, and validates/builds both the test configuration and matrix-only variant. Omit `--compile` for configuration/code-generation checks only, or use `--native` to also run native tests without building ESP32 firmware. No command uploads or flashes firmware.

The native tests deliberately log a failed capacity check to verify fail-safe behavior; that error is expected.

The GitHub Actions validation uses the GitHub release asset API for ESPHome's recommended ESP-IDF archive and Espressif's official tool mirror. This avoids transient failures from GitHub release download URLs while keeping both firmware builds required.
