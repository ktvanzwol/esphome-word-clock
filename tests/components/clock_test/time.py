import esphome.codegen as cg
from esphome.components import time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from . import TestTime

CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(TestTime)}
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await time.register_time(var, config)
