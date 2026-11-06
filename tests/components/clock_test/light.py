import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID
from esphome.types import ConfigType

from . import clock_test_ns

TestLight = clock_test_ns.class_("TestLight", light.AddressableLight)
CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TestLight),
        cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        cv.Optional("actual_leds"): cv.positive_not_null_int,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config.get("actual_leds", config[CONF_NUM_LEDS]))
    await cg.register_component(var, config)
    await light.register_light(var, config)
