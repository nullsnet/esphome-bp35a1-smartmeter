import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, text_sensor, uart
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
CONF_IPV6_ADDRESS = "ipv6_address"
CONF_DEST_IPV6_ADDRESS = "dest_ipv6_address"
CONF_MAC_ADDRESS = "mac_address"
CONF_MAC_ADDRESS_16 = "mac_address_16"
CONF_CHANNEL = "channel"
CONF_PAN_ID = "pan_id"
CONF_LQI = "lqi"
CONF_PAIR_ID = "pair_id"
CONF_SCAN_MODE = "scan_mode"

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
            cv.Optional(CONF_IPV6_ADDRESS): text_sensor.text_sensor_schema(
                icon="mdi:ip-network",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="IPv6 Address"): cv.string,
                }
            ),
            cv.Optional(CONF_DEST_IPV6_ADDRESS): text_sensor.text_sensor_schema(
                icon="mdi:ip-network",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Dest IPv6 Address"): cv.string,
                }
            ),
            cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
                icon="mdi:barcode",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="MAC Address"): cv.string,
                }
            ),
            cv.Optional(CONF_MAC_ADDRESS_16): text_sensor.text_sensor_schema(
                icon="mdi:barcode",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="MAC Address 16"): cv.string,
                }
            ),
            cv.Optional(CONF_CHANNEL): text_sensor.text_sensor_schema(
                icon="mdi:radio-tower",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Channel"): cv.string,
                }
            ),
            cv.Optional(CONF_PAN_ID): text_sensor.text_sensor_schema(
                icon="mdi:identifier",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="PAN ID"): cv.string,
                }
            ),
            cv.Optional(CONF_LQI): text_sensor.text_sensor_schema(
                icon="mdi:signal-cellular-1",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="LQI"): cv.string,
                }
            ),
            cv.Optional(CONF_PAIR_ID): text_sensor.text_sensor_schema(
                icon="mdi:key-variant",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Pair ID"): cv.string,
                }
            ),
            cv.Optional(CONF_SCAN_MODE): text_sensor.text_sensor_schema(
                icon="mdi:magnify-scan",
                entity_category="diagnostic",
            ).extend(
                {
                    cv.Optional(CONF_NAME, default="Scan Mode"): cv.string,
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

    if ipv6_conf := config.get(CONF_IPV6_ADDRESS):
        tsens = await text_sensor.new_text_sensor(ipv6_conf)
        cg.add(var.set_ipv6_address_text_sensor(tsens))

    if dest_ipv6_conf := config.get(CONF_DEST_IPV6_ADDRESS):
        tsens = await text_sensor.new_text_sensor(dest_ipv6_conf)
        cg.add(var.set_dest_ipv6_address_text_sensor(tsens))

    if mac_conf := config.get(CONF_MAC_ADDRESS):
        tsens = await text_sensor.new_text_sensor(mac_conf)
        cg.add(var.set_mac_address_text_sensor(tsens))

    if mac_16_conf := config.get(CONF_MAC_ADDRESS_16):
        tsens = await text_sensor.new_text_sensor(mac_16_conf)
        cg.add(var.set_mac_address_16_text_sensor(tsens))

    if channel_conf := config.get(CONF_CHANNEL):
        tsens = await text_sensor.new_text_sensor(channel_conf)
        cg.add(var.set_channel_text_sensor(tsens))

    if pan_id_conf := config.get(CONF_PAN_ID):
        tsens = await text_sensor.new_text_sensor(pan_id_conf)
        cg.add(var.set_pan_id_text_sensor(tsens))

    if lqi_conf := config.get(CONF_LQI):
        tsens = await text_sensor.new_text_sensor(lqi_conf)
        cg.add(var.set_lqi_text_sensor(tsens))

    if pair_id_conf := config.get(CONF_PAIR_ID):
        tsens = await text_sensor.new_text_sensor(pair_id_conf)
        cg.add(var.set_pair_id_text_sensor(tsens))

    if scan_mode_conf := config.get(CONF_SCAN_MODE):
        tsens = await text_sensor.new_text_sensor(scan_mode_conf)
        cg.add(var.set_scan_mode_text_sensor(tsens))
