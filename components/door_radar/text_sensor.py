"""door_radar text_sensors: richting en afwijzingsreden (contracts/ha-entities.md)."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_DOOR_RADAR_ID, DoorRadar

DEPENDENCIES = ["door_radar"]

CONF_DIRECTION = "direction"
CONF_REJECT_REASON = "reject_reason"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DOOR_RADAR_ID): cv.use_id(DoorRadar),
        cv.Optional(CONF_DIRECTION): text_sensor.text_sensor_schema(
            icon="mdi:arrow-left-right",
        ),
        cv.Optional(CONF_REJECT_REASON): text_sensor.text_sensor_schema(
            icon="mdi:filter-remove-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DOOR_RADAR_ID])
    if conf := config.get(CONF_DIRECTION):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(hub.set_direction_text_sensor(sens))
    if conf := config.get(CONF_REJECT_REASON):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(hub.set_reject_reason_text_sensor(sens))
