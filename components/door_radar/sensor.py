"""door_radar sensors: diagnose (positie, snelheid, energie per zone), contracts/ha-entities.md."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_SIGNAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    UNIT_METER_PER_SECOND,
)

from . import CONF_DOOR_RADAR_ID, DoorRadar

DEPENDENCIES = ["door_radar"]

CONF_POSITION = "position"
CONF_SPEED = "speed"
CONF_GATE_ENERGY = "gate_energy"
GATE_COUNT = 9

GATE_ENERGY_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon=ICON_SIGNAL,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DOOR_RADAR_ID): cv.use_id(DoorRadar),
        cv.Optional(CONF_POSITION): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER_PER_SECOND,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_GATE_ENERGY): cv.All(
            cv.ensure_list(GATE_ENERGY_SCHEMA),
            cv.Length(
                min=GATE_COUNT,
                max=GATE_COUNT,
                msg=f"gate_energy moet precies {GATE_COUNT} sensoren bevatten (zone 0-8)",
            ),
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DOOR_RADAR_ID])
    if conf := config.get(CONF_POSITION):
        sens = await sensor.new_sensor(conf)
        cg.add(hub.set_position_sensor(sens))
    if conf := config.get(CONF_SPEED):
        sens = await sensor.new_sensor(conf)
        cg.add(hub.set_speed_sensor(sens))
    for gate, conf in enumerate(config.get(CONF_GATE_ENERGY, [])):
        sens = await sensor.new_sensor(conf)
        cg.add(hub.set_gate_energy_sensor(gate, sens))
