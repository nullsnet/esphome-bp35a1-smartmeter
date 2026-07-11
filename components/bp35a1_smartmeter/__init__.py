import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    ICON_FLASH,
    ICON_ACCOUNT,
    UNIT_WATT,
    UNIT_AMPERE,
    UNIT_KILOWATT_HOURS,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
)
from pathlib import Path

CODEOWNERS = ["@nullsnet"]
DEPENDENCIES = ["binary_sensor", "sensor"]

_SRC_DIR = Path(__file__).parent

bp35a1_smartmeter_ns = cg.esphome_ns.namespace("bp35a1_smartmeter")
BP35A1SmartMeterComponent = bp35a1_smartmeter_ns.class_(
    "BP35A1SmartMeterComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_B_ROUTE_ID = "b_route_id"
CONF_B_ROUTE_PASSWORD = "b_route_password"
CONF_POWER = "power"
CONF_CURRENT_R = "current_r"
CONF_CURRENT_T = "current_t"
CONF_ENERGY = "energy"
CONF_CONNECTION = "connection"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BP35A1SmartMeterComponent),
            cv.Required(CONF_B_ROUTE_ID): cv.string_strict,
            cv.Required(CONF_B_ROUTE_PASSWORD): cv.string_strict,
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_FLASH,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Instantaneous Power"): cv.string,
                }
            ),
            cv.Optional(CONF_CURRENT_R): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Instantaneous Current R"): cv.string,
                }
            ),
            cv.Optional(CONF_CURRENT_T): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_FLASH,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Instantaneous Current T"): cv.string,
                }
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                icon=ICON_FLASH,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY,
                state_class="total_increasing",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Cumulative Energy Positive"): cv.string,
                }
            ),
            cv.Optional(CONF_CONNECTION): binary_sensor.binary_sensor_schema(
                icon=ICON_ACCOUNT,
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="B-route Connection"): cv.string,
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_b_route_id(config[CONF_B_ROUTE_ID]))
    cg.add(var.set_b_route_password(config[CONF_B_ROUTE_PASSWORD]))

    if power_conf := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_conf)
        cg.add(var.set_power_sensor(sens))

    if current_r_conf := config.get(CONF_CURRENT_R):
        sens = await sensor.new_sensor(current_r_conf)
        cg.add(var.set_current_r_sensor(sens))

    if current_t_conf := config.get(CONF_CURRENT_T):
        sens = await sensor.new_sensor(current_t_conf)
        cg.add(var.set_current_t_sensor(sens))

    if energy_conf := config.get(CONF_ENERGY):
        sens = await sensor.new_sensor(energy_conf)
        cg.add(var.set_energy_sensor(sens))

    if connection_conf := config.get(CONF_CONNECTION):
        bsens = await binary_sensor.new_binary_sensor(connection_conf)
        cg.add(var.set_connection_sensor(bsens))
