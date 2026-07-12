#include "bp35a1_smartmeter.h"

namespace esphome {
namespace bp35a1_smartmeter {

static const char *const TAG = "bp35a1_smartmeter";

void BP35A1SmartMeterComponent::setup() {
    ESP_LOGI(TAG, "BP35A1 Smart Meter Component initialized");
    ESP_LOGD(TAG, "B-route ID: %s", b_route_id_.c_str());

    uart_adapter_ = new UARTDeviceAdapter(*this);
    bp35a1_ = new BP35A1(b_route_id_, b_route_password_, *uart_adapter_);

    bp35a1_->setStatusChangeCallback([this](BP35A1::InitializeState state) {
        switch (state) {
            case BP35A1::InitializeState::readySmartMeter:
                ESP_LOGI(TAG, "Initialization complete - ready for communication");
                if (connection_sensor_) connection_sensor_->publish_state(true);
                publish_info_sensors_();
                break;
            case BP35A1::InitializeState::uninitialized:
                ESP_LOGW(TAG, "Initialization reset");
                if (connection_sensor_) connection_sensor_->publish_state(false);
                break;
            case BP35A1::InitializeState::setSKStackPassword:
                ESP_LOGI(TAG, "Setting B-route password");
                break;
            case BP35A1::InitializeState::setSKStackId:
                ESP_LOGI(TAG, "Setting B-route ID");
                break;
            case BP35A1::InitializeState::activeScanWithIE:
                ESP_LOGI(TAG, "Starting Wi-SUN scan");
                break;
            case BP35A1::InitializeState::skJoin:
                ESP_LOGI(TAG, "Joining B-route network");
                break;
            case BP35A1::InitializeState::waitPana:
                ESP_LOGI(TAG, "Waiting for PANA authentication");
                break;
            default:
                break;
        }
    });

    init_start_ms_ = 0;
}

void BP35A1SmartMeterComponent::loop() {
    if (!bp35a1_) return;

    const uint32_t now = millis();
    if (now - last_loop_ms_ < 100) return;
    last_loop_ms_ = now;

    if (bp35a1_->getInitializeState() != BP35A1::InitializeState::readySmartMeter) {
        if (init_start_ms_ == 0) {
            init_start_ms_ = now;
        } else if (now - init_start_ms_ >= 180000) {
            ESP_LOGE(TAG, "Initialization timeout (180s), restarting...");
            esp_restart();
        }
        const uint32_t panaFails = bp35a1_->getPanaFailCount();
        if (panaFails > last_pana_fail_count_) {
            ESP_LOGW(TAG, "B-route authentication failed %u time(s) - check b_route_id and b_route_password", panaFails);
            last_pana_fail_count_ = panaFails;
        }
        bp35a1_->initializeLoop();
        return;
    }

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

void BP35A1SmartMeterComponent::update() {
    if (!bp35a1_) return;
    if (bp35a1_->getInitializeState() != BP35A1::InitializeState::readySmartMeter) {
        return;
    }

    ESP_LOGI(TAG, "Sending property request...");
    bp35a1_->sendPropertyRequest({
        LowVoltageSmartElectricEnergyMeterClass::Property::InstantaneousPower,
        LowVoltageSmartElectricEnergyMeterClass::Property::InstantaneousCurrents,
        LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyPositive,
    });
}

void BP35A1SmartMeterComponent::publish_info_sensors_() {
    ESP_LOGI(TAG, "Publishing device info sensors");

    if (ipv6_address_text_sensor_) {
        ipv6_address_text_sensor_->publish_state(bp35a1_->getLocalIpv6Address());
    }
    if (dest_ipv6_address_text_sensor_) {
        dest_ipv6_address_text_sensor_->publish_state(bp35a1_->getCommunicationIpv6Address());
    }
    if (mac_address_text_sensor_) {
        mac_address_text_sensor_->publish_state(bp35a1_->getMacAddress64());
    }
    if (mac_address_16_text_sensor_) {
        mac_address_16_text_sensor_->publish_state(bp35a1_->getMacAddress16());
    }
    if (channel_sensor_) {
        channel_sensor_->publish_state(static_cast<float>(bp35a1_->getChannel()));
    }
    if (pan_id_text_sensor_) {
        pan_id_text_sensor_->publish_state(bp35a1_->getPanId());
    }
    if (lqi_sensor_) {
        lqi_sensor_->publish_state(static_cast<float>(bp35a1_->getLQI()));
    }
    if (pair_id_text_sensor_) {
        pair_id_text_sensor_->publish_state(bp35a1_->getPairId());
    }
    if (scan_mode_text_sensor_) {
        scan_mode_text_sensor_->publish_state(bp35a1_->getScanModeString());
    }
}

void BP35A1SmartMeterComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "BP35A1 Smart Meter");
    LOG_SENSOR("  ", "Power", power_sensor_);
    LOG_SENSOR("  ", "Current R", current_r_sensor_);
    LOG_SENSOR("  ", "Current T", current_t_sensor_);
    LOG_SENSOR("  ", "Energy", energy_sensor_);
    LOG_BINARY_SENSOR("  ", "Connection", connection_sensor_);
    LOG_TEXT_SENSOR("  ", "IPv6 Address", ipv6_address_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Dest IPv6 Address", dest_ipv6_address_text_sensor_);
    LOG_TEXT_SENSOR("  ", "MAC Address", mac_address_text_sensor_);
    LOG_TEXT_SENSOR("  ", "MAC Address 16", mac_address_16_text_sensor_);
    LOG_SENSOR("  ", "Channel", channel_sensor_);
    LOG_TEXT_SENSOR("  ", "PAN ID", pan_id_text_sensor_);
    LOG_SENSOR("  ", "LQI", lqi_sensor_);
    LOG_TEXT_SENSOR("  ", "Pair ID", pair_id_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Scan Mode", scan_mode_text_sensor_);
}

}  // namespace bp35a1_smartmeter
}  // namespace esphome
