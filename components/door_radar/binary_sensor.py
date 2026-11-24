"""door_radar binary_sensors: aanwezigheid en sensorstoring (contracts/ha-entities.md)."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PRESENCE,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCOUNT,
)

from . import CONF_DOOR_RADAR_ID, DoorRadar

DEPENDENCIES = ["door_radar"]

CONF_PRESENCE = "presence"
CONF_SENSOR_FAULT = "sensor_fault"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DOOR_RADAR_ID): cv.use_id(DoorRadar),
        cv.Optional(CONF_PRESENCE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PRESENCE,
            icon=ICON_ACCOUNT,
        ),
        cv.Optional(CONF_SENSOR_FAULT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DOOR_RADAR_ID])
    if conf := config.get(CONF_PRESENCE):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.set_presence_binary_sensor(sens))
    if conf := config.get(CONF_SENSOR_FAULT):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(hub.set_sensor_fault_binary_sensor(sens))
