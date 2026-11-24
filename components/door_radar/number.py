"""door_radar numbers: de 18 drempels uit FR-011 (contracts/ha-entities.md, Configuratie).

Elke number bewaart zijn waarde zelf; DoorRadar valideert bij het opstarten alle waarden samen
(T040, T041). initial_value is de standaardwaarde (data-model.md DetectorConfig).
"""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_MODE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_SPEED,
    ENTITY_CATEGORY_CONFIG,
    ICON_SIGNAL,
    UNIT_METER,
    UNIT_METER_PER_SECOND,
    UNIT_SECOND,
)

from . import CONF_DOOR_RADAR_ID, DoorRadar, door_radar_ns

DEPENDENCIES = ["door_radar"]

DoorRadarNumber = door_radar_ns.class_(
    "DoorRadarNumber", number.Number, cg.Component, cg.Parented.template(DoorRadar)
)
NumberField = door_radar_ns.enum("NumberField", is_class=True)

CONF_GATE_MIN_ENERGY = "gate_min_energy"
GATE_COUNT = 9

# sleutel: (C++-veld, eenheid, device_class, min, max, stap, standaardwaarde)
FIELDS = {
    "min_range": (NumberField.MIN_RANGE, UNIT_METER, DEVICE_CLASS_DISTANCE, 0.0, 6.0, 0.05, 0.75),
    "max_range": (NumberField.MAX_RANGE, UNIT_METER, DEVICE_CLASS_DISTANCE, 0.5, 6.0, 0.05, 5.0),
    "min_speed": (NumberField.MIN_SPEED, UNIT_METER_PER_SECOND, DEVICE_CLASS_SPEED, 0.05, 3.0, 0.05, 0.3),
    "max_speed": (NumberField.MAX_SPEED, UNIT_METER_PER_SECOND, DEVICE_CLASS_SPEED, 0.1, 5.0, 0.05, 2.5),
    "min_duration": (NumberField.MIN_DURATION, UNIT_SECOND, DEVICE_CLASS_DURATION, 0.2, 5.0, 0.1, 1.0),
    "min_displacement": (NumberField.MIN_DISPLACEMENT, UNIT_METER, DEVICE_CLASS_DISTANCE, 0.1, 3.0, 0.05, 0.5),
    "max_pause": (NumberField.MAX_PAUSE, UNIT_SECOND, DEVICE_CLASS_DURATION, 0.0, 3.0, 0.1, 1.0),
    "absent_timeout": (NumberField.ABSENT_TIMEOUT, UNIT_SECOND, DEVICE_CLASS_DURATION, 1.0, 60.0, 1.0, 5.0),
    "max_hold": (NumberField.MAX_HOLD, UNIT_SECOND, DEVICE_CLASS_DURATION, 5.0, 600.0, 5.0, 60.0),
}
GATE_LIMITS = (0.0, 100.0, 1.0, 20.0)  # min, max, stap, standaard


def _number_schema(unit, device_class, minimum, maximum, default, icon=None):
    kwargs = {"entity_category": ENTITY_CATEGORY_CONFIG}
    if unit is not None:
        kwargs["unit_of_measurement"] = unit
    if device_class is not None:
        kwargs["device_class"] = device_class
    if icon is not None:
        kwargs["icon"] = icon
    return (
        number.number_schema(DoorRadarNumber, **kwargs)
        .extend(
            {
                cv.Optional(CONF_INITIAL_VALUE, default=default): cv.float_range(
                    min=minimum, max=maximum
                ),
                cv.Optional(CONF_MODE, default="BOX"): cv.enum(
                    number.NUMBER_MODES, upper=True
                ),
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
    )


_schema = {cv.GenerateID(CONF_DOOR_RADAR_ID): cv.use_id(DoorRadar)}
for _key, (_field, _unit, _dc, _min, _max, _step, _default) in FIELDS.items():
    _schema[cv.Optional(_key)] = _number_schema(_unit, _dc, _min, _max, _default)
_schema[cv.Optional(CONF_GATE_MIN_ENERGY)] = cv.All(
    cv.ensure_list(
        _number_schema(None, None, GATE_LIMITS[0], GATE_LIMITS[1], GATE_LIMITS[3], ICON_SIGNAL)
    ),
    cv.Length(
        min=GATE_COUNT,
        max=GATE_COUNT,
        msg=f"gate_min_energy moet precies {GATE_COUNT} numbers bevatten (zone 0-8)",
    ),
)
CONFIG_SCHEMA = cv.Schema(_schema)


async def _register(hub, conf, field, gate, minimum, maximum, step):
    var = cg.new_Pvariable(conf[CONF_ID], field, gate)
    await number.register_number(var, conf, min_value=minimum, max_value=maximum, step=step)
    await cg.register_component(var, conf)
    await cg.register_parented(var, hub)
    cg.add(var.set_initial_value(conf[CONF_INITIAL_VALUE]))
    cg.add(hub.register_number(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DOOR_RADAR_ID])
    for key, (field, _unit, _dc, minimum, maximum, step, _default) in FIELDS.items():
        if conf := config.get(key):
            await _register(hub, conf, field, 0, minimum, maximum, step)
    for gate, conf in enumerate(config.get(CONF_GATE_MIN_ENERGY, [])):
        await _register(
            hub,
            conf,
            NumberField.GATE_MIN_ENERGY,
            gate,
            GATE_LIMITS[0],
            GATE_LIMITS[1],
            GATE_LIMITS[2],
        )
