"""door_radar event: één event per detectie, type = richting (contracts/ha-entities.md)."""

import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv

from . import CONF_DOOR_RADAR_ID, DoorRadar

DEPENDENCIES = ["door_radar"]

CONF_DETECTION = "detection"
# Moeten overeenkomen met presence::toString(Direction) in detector_config.cpp.
EVENT_TYPES = ["naderend", "verwijderend"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DOOR_RADAR_ID): cv.use_id(DoorRadar),
        cv.Optional(CONF_DETECTION): event.event_schema(
            event.Event,
            icon="mdi:walk",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DOOR_RADAR_ID])
    if conf := config.get(CONF_DETECTION):
        evt = await event.new_event(conf, event_types=EVENT_TYPES)
        cg.add(hub.set_detection_event(evt))
