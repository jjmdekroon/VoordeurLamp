"""ESPHome external component: persoonsdetectie met de LD2410B mmWave-radar.

Zie specs/001-mmwave-person-detection/contracts/esphome-config.md voor het schema.
"""

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = False

door_radar_ns = cg.esphome_ns.namespace("door_radar")
DoorRadar = door_radar_ns.class_("DoorRadar", cg.Component, uart.UARTDevice)

CONF_DOOR_RADAR_ID = "door_radar_id"

CONF_REVERSAL_HYSTERESIS = "reversal_hysteresis"
CONF_EVALUATION_WINDOW = "evaluation_window"
CONF_BACKGROUND_MARGIN = "background_margin"
CONF_BACKGROUND_TIME_CONSTANT = "background_time_constant"
CONF_STARTUP_TIME = "startup_time"
CONF_SENSOR_TIMEOUT = "sensor_timeout"
CONF_ACK_TIMEOUT = "ack_timeout"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DoorRadar),
            cv.Optional(CONF_REVERSAL_HYSTERESIS, default="0.3m"): cv.All(
                cv.distance, cv.float_range(min=0.05, max=3.0)
            ),
            cv.Optional(CONF_EVALUATION_WINDOW, default="1500ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=200),
                    max=cv.TimePeriod(milliseconds=5000),
                ),
            ),
            cv.Optional(CONF_BACKGROUND_MARGIN, default=15): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_BACKGROUND_TIME_CONSTANT, default="30s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(seconds=3600)
                ),
            ),
            cv.Optional(CONF_STARTUP_TIME, default="5s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(seconds=60)),
            ),
            # FR-013: een sensorfout MOET binnen 3 s gedetecteerd worden.
            cv.Optional(CONF_SENSOR_TIMEOUT, default="3s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=500),
                    max=cv.TimePeriod(seconds=3),
                ),
            ),
            cv.Optional(CONF_ACK_TIMEOUT, default="500ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=50),
                    max=cv.TimePeriod(seconds=5),
                ),
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "door_radar",
    baud_rate=256000,
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_reversal_hysteresis_m(config[CONF_REVERSAL_HYSTERESIS]))
    cg.add(
        var.set_evaluation_window_ms(
            config[CONF_EVALUATION_WINDOW].total_milliseconds
        )
    )
    cg.add(var.set_background_margin(config[CONF_BACKGROUND_MARGIN]))
    cg.add(
        var.set_background_time_constant_ms(
            config[CONF_BACKGROUND_TIME_CONSTANT].total_milliseconds
        )
    )
    cg.add(var.set_startup_ms(config[CONF_STARTUP_TIME].total_milliseconds))
    cg.add(var.set_sensor_timeout_ms(config[CONF_SENSOR_TIMEOUT].total_milliseconds))
    cg.add(var.set_ack_timeout_ms(config[CONF_ACK_TIMEOUT].total_milliseconds))
