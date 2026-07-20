#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "BP35A1.hpp"
#include "UARTDeviceAdapter.h"

namespace esphome {
namespace bp35a1_smartmeter {

class BP35A1SmartMeterComponent : public PollingComponent, public uart::UARTDevice {
  public:
    BP35A1SmartMeterComponent() = default;

    void set_b_route_id(const std::string &id) { b_route_id_ = id; }
    void set_b_route_password(const std::string &password) { b_route_password_ = password; }
    void set_init_timeout(uint32_t ms) { init_timeout_ms_ = ms; }
    void set_loop_interval(uint32_t ms) { loop_interval_ms_ = ms; }
    void set_scan_channel_mask(uint32_t mask) { scan_channel_mask_ = mask; }
    void set_info_batch_delay(uint32_t ms) { info_batch_delay_ms_ = ms; }
    void set_info_batch_timeout(uint32_t ms) { info_batch_timeout_ms_ = ms; }

    void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
    void set_current_r_sensor(sensor::Sensor *s) { current_r_sensor_ = s; }
    void set_current_t_sensor(sensor::Sensor *s) { current_t_sensor_ = s; }
    void set_energy_sensor(sensor::Sensor *s) { energy_sensor_ = s; }
    void set_energy_reverse_sensor(sensor::Sensor *s) { energy_reverse_sensor_ = s; }
    void set_connection_sensor(binary_sensor::BinarySensor *s) { connection_sensor_ = s; }

    void set_ipv6_address_text_sensor(text_sensor::TextSensor *s) { ipv6_address_text_sensor_ = s; }
    void set_dest_ipv6_address_text_sensor(text_sensor::TextSensor *s) { dest_ipv6_address_text_sensor_ = s; }
    void set_mac_address_text_sensor(text_sensor::TextSensor *s) { mac_address_text_sensor_ = s; }
    void set_mac_address_16_text_sensor(text_sensor::TextSensor *s) { mac_address_16_text_sensor_ = s; }
    void set_channel_sensor(sensor::Sensor *s) { channel_sensor_ = s; }
    void set_pan_id_text_sensor(text_sensor::TextSensor *s) { pan_id_text_sensor_ = s; }
    void set_lqi_sensor(sensor::Sensor *s) { lqi_sensor_ = s; }
    void set_pair_id_text_sensor(text_sensor::TextSensor *s) { pair_id_text_sensor_ = s; }
    void set_scan_mode_text_sensor(text_sensor::TextSensor *s) { scan_mode_text_sensor_ = s; }
    void set_installation_location_text_sensor(text_sensor::TextSensor *s) { installation_location_text_sensor_ = s; }
    void set_standard_version_information_text_sensor(text_sensor::TextSensor *s) { standard_version_information_text_sensor_ = s; }
    void set_manufacturer_code_text_sensor(text_sensor::TextSensor *s) { manufacturer_code_text_sensor_ = s; }
    void set_production_number_text_sensor(text_sensor::TextSensor *s) { production_number_text_sensor_ = s; }
    void set_get_property_map_text_sensor(text_sensor::TextSensor *s) { get_property_map_text_sensor_ = s; }
    void set_operation_status_text_sensor(text_sensor::TextSensor *s) { operation_status_text_sensor_ = s; }
    void set_fault_status_text_sensor(text_sensor::TextSensor *s) { fault_status_text_sensor_ = s; }
    void set_current_time_setting_text_sensor(text_sensor::TextSensor *s) { current_time_setting_text_sensor_ = s; }
    void set_current_date_setting_text_sensor(text_sensor::TextSensor *s) { current_date_setting_text_sensor_ = s; }
    void set_status_change_announcement_property_map_text_sensor(text_sensor::TextSensor *s) { status_change_announcement_property_map_text_sensor_ = s; }
    void set_coefficient_text_sensor(text_sensor::TextSensor *s) { coefficient_text_sensor_ = s; }
    void set_cumulative_energy_effective_digits_text_sensor(text_sensor::TextSensor *s) { cumulative_energy_effective_digits_text_sensor_ = s; }
    void set_cumulative_energy_unit_text_sensor(text_sensor::TextSensor *s) { cumulative_energy_unit_text_sensor_ = s; }
    void set_cumulative_energy_history_positive_text_sensor(text_sensor::TextSensor *s) { cumulative_energy_history_positive_text_sensor_ = s; }
    void set_cumulative_energy_history_negative_text_sensor(text_sensor::TextSensor *s) { cumulative_energy_history_negative_text_sensor_ = s; }
    void set_date_of_collect_cumulative_energy_history_text_sensor(text_sensor::TextSensor *s) { date_of_collect_cumulative_energy_history_text_sensor_ = s; }
    void set_fixed_cumulative_energy_positive_text_sensor(text_sensor::TextSensor *s) { fixed_cumulative_energy_positive_text_sensor_ = s; }
    void set_fixed_cumulative_energy_negative_text_sensor(text_sensor::TextSensor *s) { fixed_cumulative_energy_negative_text_sensor_ = s; }
    void set_cumulative_energy_history2_text_sensor(text_sensor::TextSensor *s) { cumulative_energy_history2_text_sensor_ = s; }
    void set_date_of_collect_cumulative_energy_history2_text_sensor(text_sensor::TextSensor *s) { date_of_collect_cumulative_energy_history2_text_sensor_ = s; }

    void setup() override;
    void update() override;
    void loop() override;
    void dump_config() override;

  protected:
    void publish_info_sensors_();
    void publish_meter_info_sensors_(const LowVoltageSmartElectricEnergyMeterClass &meter);
    void build_info_batches_(const std::vector<uint8_t> &decoded_property_map);

    std::string b_route_id_;
    std::string b_route_password_;

    sensor::Sensor *power_sensor_{nullptr};
    sensor::Sensor *current_r_sensor_{nullptr};
    sensor::Sensor *current_t_sensor_{nullptr};
    sensor::Sensor *energy_sensor_{nullptr};
    sensor::Sensor *energy_reverse_sensor_{nullptr};
    binary_sensor::BinarySensor *connection_sensor_{nullptr};

    text_sensor::TextSensor *ipv6_address_text_sensor_{nullptr};
    text_sensor::TextSensor *dest_ipv6_address_text_sensor_{nullptr};
    text_sensor::TextSensor *mac_address_text_sensor_{nullptr};
    text_sensor::TextSensor *mac_address_16_text_sensor_{nullptr};
    sensor::Sensor *channel_sensor_{nullptr};
    text_sensor::TextSensor *pan_id_text_sensor_{nullptr};
    sensor::Sensor *lqi_sensor_{nullptr};
    text_sensor::TextSensor *pair_id_text_sensor_{nullptr};
    text_sensor::TextSensor *scan_mode_text_sensor_{nullptr};
    text_sensor::TextSensor *installation_location_text_sensor_{nullptr};
    text_sensor::TextSensor *standard_version_information_text_sensor_{nullptr};
    text_sensor::TextSensor *manufacturer_code_text_sensor_{nullptr};
    text_sensor::TextSensor *production_number_text_sensor_{nullptr};
    text_sensor::TextSensor *get_property_map_text_sensor_{nullptr};
    text_sensor::TextSensor *operation_status_text_sensor_{nullptr};
    text_sensor::TextSensor *fault_status_text_sensor_{nullptr};
    text_sensor::TextSensor *current_time_setting_text_sensor_{nullptr};
    text_sensor::TextSensor *current_date_setting_text_sensor_{nullptr};
    text_sensor::TextSensor *status_change_announcement_property_map_text_sensor_{nullptr};
    text_sensor::TextSensor *coefficient_text_sensor_{nullptr};
    text_sensor::TextSensor *cumulative_energy_effective_digits_text_sensor_{nullptr};
    text_sensor::TextSensor *cumulative_energy_unit_text_sensor_{nullptr};
    text_sensor::TextSensor *cumulative_energy_history_positive_text_sensor_{nullptr};
    text_sensor::TextSensor *cumulative_energy_history_negative_text_sensor_{nullptr};
    text_sensor::TextSensor *date_of_collect_cumulative_energy_history_text_sensor_{nullptr};
    text_sensor::TextSensor *fixed_cumulative_energy_positive_text_sensor_{nullptr};
    text_sensor::TextSensor *fixed_cumulative_energy_negative_text_sensor_{nullptr};
    text_sensor::TextSensor *cumulative_energy_history2_text_sensor_{nullptr};
    text_sensor::TextSensor *date_of_collect_cumulative_energy_history2_text_sensor_{nullptr};

    UARTDeviceAdapter *uart_adapter_{nullptr};
    BP35A1 *bp35a1_{nullptr};

    uint32_t init_start_ms_{0};
    uint32_t last_loop_ms_{0};
    uint32_t last_pana_fail_count_{0};
    uint32_t init_timeout_ms_{180000};
    uint32_t loop_interval_ms_{100};
    uint32_t scan_channel_mask_{0xFFFFFFFF};
    uint32_t info_batch_delay_ms_{500};
    uint32_t info_batch_timeout_ms_{5000};
    uint32_t info_batch_last_send_ms_{0};
    uint32_t info_batch_sent_ms_{0};
    bool info_sensors_published_{false};
    uint8_t info_request_step_{0};
    std::vector<std::vector<uint8_t>> info_batches_;
    std::vector<std::vector<uint8_t>> info_retry_batches_;
    uint8_t info_retry_step_{0};
};

}  // namespace bp35a1_smartmeter
}  // namespace esphome
