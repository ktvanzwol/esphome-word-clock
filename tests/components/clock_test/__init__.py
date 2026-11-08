import esphome.codegen as cg
from esphome.components import light, time
from esphome.components.addressable_light.display import AddressableLightDisplay
from esphome.components.word_clock.light import WordClock
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID
from esphome.types import ConfigType

DOMAIN = "clock_test"
clock_test_ns = cg.esphome_ns.namespace(DOMAIN)
TestTime = clock_test_ns.class_("TestTime", time.RealTimeClock)
TestHarness = clock_test_ns.class_("TestHarness", cg.Component)

REFERENCES = {
    "clock_id": WordClock,
    "matrix_only_clock_id": WordClock,
    "light_id": light.LightState,
    "matrix_only_light_id": light.LightState,
    CONF_TIME_ID: TestTime,
    "matrix_id": AddressableLightDisplay,
    "dots_id": AddressableLightDisplay,
    "matrix_only_display_id": AddressableLightDisplay,
    "failed_clock_id": WordClock,
    "failed_display_id": AddressableLightDisplay,
}
CONFIG_SCHEMA = cv.Schema(
    {cv.GenerateID(): cv.declare_id(TestHarness), **{cv.Required(key): cv.use_id(kind) for key, kind in REFERENCES.items()}}
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    references = [await cg.get_variable(config[key]) for key in REFERENCES]
    var = cg.new_Pvariable(config[CONF_ID], *references)
    await cg.register_component(var, config)
