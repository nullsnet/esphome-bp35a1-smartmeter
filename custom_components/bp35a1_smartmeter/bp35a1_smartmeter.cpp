#include "bp35a1_smartmeter.h"

namespace esphome {
namespace bp35a1_smartmeter {

static const char *const TAG = "bp35a1_smartmeter";

void BP35A1SmartMeterComponent::setup() {
    ESP_LOGI(TAG, "BP35A1 Smart Meter Component initialized");
    ESP_LOGI(TAG, "B-route ID: %s", b_route_id_.c_str());

    uart_adapter_ = new UARTDeviceAdapter(*this);
    bp35a1_ = new BP35A1(b_route_id_, b_route_password_, *uart_adapter_);

    bp35a1_->setStatusChangeCallback([this](BP35A1::InitializeState state) {
        switch (state) {
            case BP35A1::InitializeState::readySmartMeter:
                ESP_LOGI(TAG, "Initialization complete - ready for communication");
                if (connection_sensor_) connection_sensor_->publish_state(true);
                break;
            case BP35A1::InitializeState::uninitialized:
                ESP_LOGW(TAG, "Initialization reset");
                if (connection_sensor_) connection_sensor_->publish_state(false);
                break;
            default:
                break;
        }
    });

    init_start_ms_ = 0;
}

void BP35A1SmartMeterComponent::update() {
    if (!bp35a1_) return;
    if (bp35a1_->getInitializeState() != BP35A1::InitializeState::readySmartMeter) return;

    bp35a1_->communicationLoop(
        [this](const LowVoltageSmartElectricEnergyMeterClass &meter) {
            int32_t power;
            float currentR, currentT, energy;

            if (meter.getInstantaneousPower(&power) &&
                meter.getInstantaneousCurrent(&currentR, &currentT) &&
                meter.getCumulativeEnergyPositive(&energy)) {
                ESP_LOGI(TAG, "Power  : %d W", power);
                ESP_LOGI(TAG, "Energy : %.3f kWh", energy);
                ESP_LOGI(TAG, "Current R: %.1f A", currentR);
                ESP_LOGI(TAG, "Current T: %.1f A", currentT);

                if (power_sensor_) power_sensor_->publish_state(static_cast<float>(power));
                if (current_r_sensor_) current_r_sensor_->publish_state(currentR);
                if (current_t_sensor_) current_t_sensor_->publish_state(currentT);
                if (energy_sensor_) energy_sensor_->publish_state(energy);
            }
        },
        BP35A1::CommunicationState::ready
    );
}

void BP35A1SmartMeterComponent::loop() {
    if (!bp35a1_) return;

    if (bp35a1_->getInitializeState() != BP35A1::InitializeState::readySmartMeter) {
        if (init_start_ms_ == 0) {
            init_start_ms_ = millis();
        } else if (millis() - init_start_ms_ >= 180000) {
            ESP_LOGE(TAG, "Initialization timeout (180s), restarting...");
            esp_restart();
        }
        bp35a1_->initializeLoop();
        return;
    }
}

void BP35A1SmartMeterComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "BP35A1 Smart Meter");
    LOG_SENSOR("  ", "Power", power_sensor_);
    LOG_SENSOR("  ", "Current R", current_r_sensor_);
    LOG_SENSOR("  ", "Current T", current_t_sensor_);
    LOG_SENSOR("  ", "Energy", energy_sensor_);
    LOG_BINARY_SENSOR("  ", "Connection", connection_sensor_);
}

}  // namespace bp35a1_smartmeter
}  // namespace esphome