#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
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

    void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
    void set_current_r_sensor(sensor::Sensor *s) { current_r_sensor_ = s; }
    void set_current_t_sensor(sensor::Sensor *s) { current_t_sensor_ = s; }
    void set_energy_sensor(sensor::Sensor *s) { energy_sensor_ = s; }
    void set_connection_sensor(binary_sensor::BinarySensor *s) { connection_sensor_ = s; }

    void setup() override;
    void update() override;
    void loop() override;
    void dump_config() override;

  protected:
    std::string b_route_id_;
    std::string b_route_password_;

    sensor::Sensor *power_sensor_{nullptr};
    sensor::Sensor *current_r_sensor_{nullptr};
    sensor::Sensor *current_t_sensor_{nullptr};
    sensor::Sensor *energy_sensor_{nullptr};
    binary_sensor::BinarySensor *connection_sensor_{nullptr};

    UARTDeviceAdapter *uart_adapter_{nullptr};
    BP35A1 *bp35a1_{nullptr};

    uint32_t init_start_ms_{0};
};

}  // namespace bp35a1_smartmeter
}  // namespace esphome
