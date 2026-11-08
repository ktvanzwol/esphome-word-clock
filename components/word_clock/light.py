"""Validate and generate the Dutch clock's native RGB light and time-based effects."""

import esphome.codegen as cg
from esphome.components import light, time
from esphome.components.addressable_light.display import AddressableLightDisplay
from esphome.components.light.effects import register_rgb_effect
from esphome.components.light.types import LightEffect
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESSABLE_LIGHT_ID,
    CONF_EFFECTS,
    CONF_GAMMA_CORRECT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INTERNAL,
    CONF_LAMBDA,
    CONF_NAME,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PAGES,
    CONF_ROTATION,
    CONF_TIME_ID,
    CONF_WIDTH,
)
import esphome.final_validate as fv
from esphome.core import CORE
from esphome.types import ConfigType, ID

DEPENDENCIES = ["time", "display"]

CONF_MATRIX_DISPLAY_ID = "matrix_display_id"
CONF_DOTS_DISPLAY_ID = "dots_display_id"
CONF_PERIOD = "period"

word_clock_ns = cg.esphome_ns.namespace("word_clock")
WordClock = word_clock_ns.class_("WordClock", cg.PollingComponent, light.LightOutput)
ColorCycleEffect = word_clock_ns.class_("ColorCycleEffect", LightEffect)
CyclePeriod = word_clock_ns.enum("CyclePeriod", is_class=True)
PERIODS = {
    "daily": CyclePeriod.DAILY,
    "hourly": CyclePeriod.HOURLY,
    "minute": CyclePeriod.MINUTE,
}


@register_rgb_effect(
    "word_clock_color_cycle",
    ColorCycleEffect,
    "Clock Color Cycle",
    {
        cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_PERIOD): cv.enum(PERIODS, lower=True),
    },
)
async def color_cycle_to_code(config: ConfigType, effect_id: ID) -> cg.MockObj:
    """Construct a native RGB effect with an explicit clock and cycle period."""
    clock = await cg.get_variable(config[CONF_TIME_ID])
    return cg.new_Pvariable(effect_id, config[CONF_NAME], clock, config[CONF_PERIOD])


CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WordClock),
        cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_MATRIX_DISPLAY_ID): cv.use_id(AddressableLightDisplay),
        cv.Optional(CONF_DOTS_DISPLAY_ID): cv.use_id(AddressableLightDisplay),
    }
).extend(cv.COMPONENT_SCHEMA)


def display_config(display_id: ID) -> ConfigType:
    """Resolve a display declaration during final validation, not code generation."""
    full_config = fv.full_config.get()
    return full_config.get_config_for_path(full_config.get_path_for_id(display_id)[:-1])


def validate_display(config: ConfigType, key: str, width: int, height: int) -> None:
    """Check rotated dimensions, single-writer ownership, and backing-light correction.

    Ownership is recorded in ESPHome's per-validation data so multiple clock
    instances cannot claim the same display or output. Output sizes unavailable
    in YAML are checked by the C++ component after hardware setup.
    """
    full_config = fv.full_config.get()
    display = display_config(config[key])
    rotation = display.get(CONF_ROTATION, 0)
    logical_width, logical_height = display[CONF_WIDTH], display[CONF_HEIGHT]
    if rotation in (90, 270):
        logical_width, logical_height = logical_height, logical_width
    if (logical_width, logical_height) != (width, height):
        raise cv.Invalid(f"{key} must have logical dimensions {width}x{height}", [key])
    if CONF_LAMBDA in display or CONF_PAGES in display:
        raise cv.Invalid("Word clock displays cannot have a lambda or pages", [key])

    backing_id = display[CONF_ADDRESSABLE_LIGHT_ID]
    backing = full_config.get_config_for_path(full_config.get_path_for_id(backing_id)[:-1])
    if not backing[CONF_INTERNAL]:
        raise cv.Invalid("Word clock backing lights must be internal", [key])
    if backing[CONF_GAMMA_CORRECT] != 1.0:
        raise cv.Invalid("Set backing light gamma_correct: 1.0; the word clock light applies gamma", [key])
    if backing.get(CONF_EFFECTS):
        raise cv.Invalid("Word clock backing lights cannot have effects", [key])
    if CONF_NUM_LEDS in backing and backing[CONF_NUM_LEDS] < width * height:
        raise cv.Invalid("Backing light has fewer LEDs than the display needs", [key])

    owners = full_config.data.setdefault("word_clock.owners", {})
    for resource in (str(config[key]), str(backing_id)):
        owner = owners.get(resource)
        if owner is not None and owner != (str(config[CONF_OUTPUT_ID]), key):
            raise cv.Invalid("Word clock displays and backing lights cannot be shared", [key])
        owners[resource] = (str(config[CONF_OUTPUT_ID]), key)

    for other in full_config.get("display", []):
        other_id = other.get(CONF_ID)
        if (
            other_id is not None
            and other_id != config[key]
            and other.get(CONF_ADDRESSABLE_LIGHT_ID) == backing_id
        ):
            raise cv.Invalid("Backing light is already used by another display", [key])


def final_validate(config: ConfigType) -> None:
    """Check cross-component constraints once all referenced IDs are resolved."""
    validate_display(config, CONF_MATRIX_DISPLAY_ID, 11, 10)
    if CONF_DOTS_DISPLAY_ID in config:
        validate_display(config, CONF_DOTS_DISPLAY_ID, 4, 1)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    """Register the light/component and derive backing-light references from displays.

    Use CORE.config here: the final-validation context is no longer active.
    Users specify each display once, without duplicating its hardware light ID.
    """
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)
    cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))
    for key, setter in (
        (CONF_MATRIX_DISPLAY_ID, var.set_matrix),
        (CONF_DOTS_DISPLAY_ID, var.set_dots),
    ):
        if key in config:
            display = await cg.get_variable(config[key])
            configured_display = next(
                (item for item in CORE.config["display"] if item.get(CONF_ID) == config[key]),
                None,
            )
            if configured_display is None:
                raise cv.Invalid(f"display {config[key]} not found in display:")
            backing = await cg.get_variable(configured_display[CONF_ADDRESSABLE_LIGHT_ID])
            cg.add(setter(display, backing))
