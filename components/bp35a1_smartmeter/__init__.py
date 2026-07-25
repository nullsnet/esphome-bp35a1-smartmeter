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
    DEVICE_CLASS_CONNECTIVITY,
)

CODEOWNERS = ["@nullsnet"]
DEPENDENCIES = ["binary_sensor", "sensor"]

bp35a1_smartmeter_ns = cg.esphome_ns.namespace("bp35a1_smartmeter")
BP35A1SmartMeterComponent = bp35a1_smartmeter_ns.class_(
    "BP35A1SmartMeterComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_B_ROUTE_ID = "b_route_id"
CONF_B_ROUTE_PASSWORD = "b_route_password"
CONF_INIT_TIMEOUT = "init_timeout"
CONF_LOOP_INTERVAL = "loop_interval"
CONF_SCAN_CHANNEL_MASK = "scan_channel_mask"
CONF_INFO_BATCH_DELAY = "info_batch_delay"

CONF_POWER = "power"
CONF_CURRENT_R = "current_r"
CONF_CURRENT_T = "current_t"
CONF_ENERGY = "energy"
CONF_ENERGY_REVERSE = "energy_reverse"
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

METER_INFO_SENSORS = [
    ("operation_status", 0x80, "Operation Status", "mdi:power"),
    ("installation_location", 0x81, "Installation Location", "mdi:map-marker"),
    ("standard_version_information", 0x82, "Standard Version Information", "mdi:information"),
    ("fault_status", 0x88, "Fault Status", "mdi:alert"),
    ("manufacturer_code", 0x8A, "Manufacturer Code", "mdi:factory"),
    ("production_number", 0x8D, "Production Number", "mdi:numeric"),
    ("current_time_setting", 0x97, "Current Time Setting", "mdi:clock"),
    ("current_date_setting", 0x98, "Current Date Setting", "mdi:calendar"),
    ("status_change_announcement_property_map", 0x9D, "Status Change Announcement PM", "mdi:map-legend"),
    ("get_property_map", 0x9F, "Get Property Map", "mdi:map"),
    ("coefficient", 0xD3, "Coefficient", "mdi:multiplication"),
    ("cumulative_energy_effective_digits", 0xD7, "Cumulative Energy Effective Digits", "mdi:alpha-k"),
    ("cumulative_energy_unit", 0xE1, "Cumulative Energy Unit", "mdi:alpha-k"),
    ("cumulative_energy_history_positive", 0xE2, "Cumulative Energy History Positive", "mdi:history"),
    ("cumulative_energy_history_negative", 0xE4, "Cumulative Energy History Negative", "mdi:history"),
    ("date_of_collect_cumulative_energy_history", 0xE5, "Date of Collect Cumulative Energy History", "mdi:calendar-clock"),
    ("fixed_cumulative_energy_positive", 0xEA, "Fixed Cumulative Energy Positive", "mdi:flash"),
    ("fixed_cumulative_energy_negative", 0xEB, "Fixed Cumulative Energy Negative", "mdi:flash"),
    ("cumulative_energy_history2", 0xEC, "Cumulative Energy History2", "mdi:history"),
    ("date_of_collect_cumulative_energy_history2", 0xED, "Date of Collect Cumulative Energy History2", "mdi:calendar-clock"),
]


def _meter_info_schema(key, name, icon):
    return text_sensor.text_sensor_schema(
        icon=icon, entity_category="diagnostic",
    ).extend({cv.Optional(CONF_NAME, default=name): cv.string})


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BP35A1SmartMeterComponent),
            cv.Required(CONF_B_ROUTE_ID): cv.string_strict,
            cv.Required(CONF_B_ROUTE_PASSWORD): cv.string_strict,
            cv.Optional(CONF_INIT_TIMEOUT, default="180s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_LOOP_INTERVAL, default="100ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SCAN_CHANNEL_MASK, default=0xFFFFFFFF): cv.hex_uint32_t,
            cv.Optional(CONF_INFO_BATCH_DELAY, default="500ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT, icon=ICON_FLASH, accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER, state_class=STATE_CLASS_MEASUREMENT,
            ).extend({cv.Optional(CONF_NAME, default="Instantaneous Power"): cv.string}),
            cv.Optional(CONF_CURRENT_R): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE, icon=ICON_FLASH, accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT, state_class=STATE_CLASS_MEASUREMENT,
            ).extend({cv.Optional(CONF_NAME, default="Instantaneous Current R"): cv.string}),
            cv.Optional(CONF_CURRENT_T): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE, icon=ICON_FLASH, accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT, state_class=STATE_CLASS_MEASUREMENT,
            ).extend({cv.Optional(CONF_NAME, default="Instantaneous Current T"): cv.string}),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS, icon=ICON_FLASH, accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY, state_class="total_increasing",
            ).extend({cv.Optional(CONF_NAME, default="Cumulative Energy Positive"): cv.string}),
            cv.Optional(CONF_ENERGY_REVERSE): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS, icon="mdi:flash", accuracy_decimals=3,
                device_class=DEVICE_CLASS_ENERGY, state_class="total_increasing",
            ).extend({cv.Optional(CONF_NAME, default="Cumulative Energy Negative"): cv.string}),
            cv.Optional(CONF_CONNECTION): binary_sensor.binary_sensor_schema(
                icon=ICON_ACCOUNT, device_class=DEVICE_CLASS_CONNECTIVITY,
            ).extend({cv.Optional(CONF_NAME, default="B-route Connection"): cv.string}),
            cv.Optional(CONF_IPV6_ADDRESS): text_sensor.text_sensor_schema(
                icon="mdi:ip-network", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="IPv6 Address"): cv.string}),
            cv.Optional(CONF_DEST_IPV6_ADDRESS): text_sensor.text_sensor_schema(
                icon="mdi:ip-network", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="Dest IPv6 Address"): cv.string}),
            cv.Optional(CONF_MAC_ADDRESS): text_sensor.text_sensor_schema(
                icon="mdi:barcode", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="MAC Address"): cv.string}),
            cv.Optional(CONF_MAC_ADDRESS_16): text_sensor.text_sensor_schema(
                icon="mdi:barcode", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="MAC Address 16"): cv.string}),
            cv.Optional(CONF_CHANNEL): sensor.sensor_schema(
                unit_of_measurement="", icon="mdi:radio-tower", accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT, entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="Channel"): cv.string}),
            cv.Optional(CONF_PAN_ID): text_sensor.text_sensor_schema(
                icon="mdi:identifier", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="PAN ID"): cv.string}),
            cv.Optional(CONF_LQI): sensor.sensor_schema(
                unit_of_measurement="", icon="mdi:signal-cellular-1", accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT, entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="LQI"): cv.string}),
            cv.Optional(CONF_PAIR_ID): text_sensor.text_sensor_schema(
                icon="mdi:key-variant", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="Pair ID"): cv.string}),
            cv.Optional(CONF_SCAN_MODE): text_sensor.text_sensor_schema(
                icon="mdi:magnify-scan", entity_category="diagnostic",
            ).extend({cv.Optional(CONF_NAME, default="Scan Mode"): cv.string}),
            **{cv.Optional(k): _meter_info_schema(k, n, i) for k, _, n, i in METER_INFO_SENSORS},
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
    cg.add(var.set_init_timeout(config[CONF_INIT_TIMEOUT]))
    cg.add(var.set_loop_interval(config[CONF_LOOP_INTERVAL]))
    cg.add(var.set_scan_channel_mask(config[CONF_SCAN_CHANNEL_MASK]))
    cg.add(var.set_info_batch_delay(config[CONF_INFO_BATCH_DELAY]))

    if power_conf := config.get(CONF_POWER):
        cg.add(var.set_power_sensor(await sensor.new_sensor(power_conf)))
    if current_r_conf := config.get(CONF_CURRENT_R):
        cg.add(var.set_current_r_sensor(await sensor.new_sensor(current_r_conf)))
    if current_t_conf := config.get(CONF_CURRENT_T):
        cg.add(var.set_current_t_sensor(await sensor.new_sensor(current_t_conf)))
    if energy_conf := config.get(CONF_ENERGY):
        cg.add(var.set_energy_sensor(await sensor.new_sensor(energy_conf)))
    if energy_reverse_conf := config.get(CONF_ENERGY_REVERSE):
        cg.add(var.set_energy_reverse_sensor(await sensor.new_sensor(energy_reverse_conf)))
    if connection_conf := config.get(CONF_CONNECTION):
        cg.add(var.set_connection_sensor(await binary_sensor.new_binary_sensor(connection_conf)))

    if ipv6_conf := config.get(CONF_IPV6_ADDRESS):
        cg.add(var.set_ipv6_address_text_sensor(await text_sensor.new_text_sensor(ipv6_conf)))
    if dest_ipv6_conf := config.get(CONF_DEST_IPV6_ADDRESS):
        cg.add(var.set_dest_ipv6_address_text_sensor(await text_sensor.new_text_sensor(dest_ipv6_conf)))
    if mac_conf := config.get(CONF_MAC_ADDRESS):
        cg.add(var.set_mac_address_text_sensor(await text_sensor.new_text_sensor(mac_conf)))
    if mac_16_conf := config.get(CONF_MAC_ADDRESS_16):
        cg.add(var.set_mac_address_16_text_sensor(await text_sensor.new_text_sensor(mac_16_conf)))
    if channel_conf := config.get(CONF_CHANNEL):
        cg.add(var.set_channel_sensor(await sensor.new_sensor(channel_conf)))
    if pan_id_conf := config.get(CONF_PAN_ID):
        cg.add(var.set_pan_id_text_sensor(await text_sensor.new_text_sensor(pan_id_conf)))
    if lqi_conf := config.get(CONF_LQI):
        cg.add(var.set_lqi_sensor(await sensor.new_sensor(lqi_conf)))
    if pair_id_conf := config.get(CONF_PAIR_ID):
        cg.add(var.set_pair_id_text_sensor(await text_sensor.new_text_sensor(pair_id_conf)))
    if scan_mode_conf := config.get(CONF_SCAN_MODE):
        cg.add(var.set_scan_mode_text_sensor(await text_sensor.new_text_sensor(scan_mode_conf)))

    for key, epc, name, _ in METER_INFO_SENSORS:
        if key_conf := config.get(key):
            cg.add(var.set_info_text_sensor(epc, await text_sensor.new_text_sensor(key_conf)))
