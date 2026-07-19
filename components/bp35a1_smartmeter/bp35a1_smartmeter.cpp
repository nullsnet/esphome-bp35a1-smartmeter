#include "bp35a1_smartmeter.h"

namespace esphome {
namespace bp35a1_smartmeter {

static const char *const TAG = "bp35a1_smartmeter";

static std::string bytes_to_hex(const std::vector<uint8_t> &bytes) {
    std::string hex = "0x";
    for (const uint8_t &b : bytes) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", b);
        hex += buf;
    }
    return hex;
}

void BP35A1SmartMeterComponent::setup() {
    ESP_LOGI(TAG, "BP35A1 Smart Meter Component initialized");
    ESP_LOGD(TAG, "B-route ID: %s", b_route_id_.c_str());
    ESP_LOGD(TAG, "Scan channel mask: 0x%08X", scan_channel_mask_);

    uart_adapter_ = new UARTDeviceAdapter(*this);
    bp35a1_ = new BP35A1(b_route_id_, b_route_password_, *uart_adapter_);
    bp35a1_->setScanChannelMask(scan_channel_mask_);

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
    if (now - last_loop_ms_ < loop_interval_ms_) return;
    last_loop_ms_ = now;

    if (bp35a1_->getInitializeState() != BP35A1::InitializeState::readySmartMeter) {
        if (init_start_ms_ == 0) {
            init_start_ms_ = now;
            ESP_LOGD(TAG, "Init started, state=%u", (uint8_t)bp35a1_->getInitializeState());
        } else if (now - init_start_ms_ >= init_timeout_ms_) {
            ESP_LOGE(TAG, "Initialization timeout (%us), restarting...", init_timeout_ms_ / 1000);
            esp_restart();
        } else {
            const uint32_t elapsed = (now - init_start_ms_) / 1000;
            ESP_LOGD(TAG, "Init progress: %us / %us, state=%u", elapsed, init_timeout_ms_ / 1000, (uint8_t)bp35a1_->getInitializeState());
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
            float energyReverse;

            if (meter.getInstantaneousPower(&power) &&
                meter.getInstantaneousCurrent(&currentR, &currentT) &&
                meter.getCumulativeEnergyPositive(&energy)) {
                if (power_sensor_) power_sensor_->publish_state(static_cast<float>(power));
                if (current_r_sensor_) current_r_sensor_->publish_state(currentR);
                if (current_t_sensor_) current_t_sensor_->publish_state(currentT);
                if (energy_sensor_) energy_sensor_->publish_state(energy);
            }
            if (meter.getCumulativeEnergyNegative(&energyReverse)) {
                if (energy_reverse_sensor_) energy_reverse_sensor_->publish_state(energyReverse);
            }

            if (info_request_sent_ && !info_sensors_published_) {
                publish_meter_info_sensors_(meter);
                info_sensors_published_ = true;
                ESP_LOGI(TAG, "Meter info sensors published");
            }
        },
        BP35A1::CommunicationState::ready
    );

    if (!info_request_sent_) {
        ESP_LOGI(TAG, "Requesting meter info properties...");
        bp35a1_->sendPropertyRequest({
            EchonetLite::Property::InstallationLocation,
            EchonetLite::Property::StandardVersionInformation,
            EchonetLite::Property::ManufacturerCode,
            EchonetLite::Property::GetPropertyMap,
        });
        info_request_sent_ = true;
    }
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
        LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyNegative,
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
        channel_sensor_->publish_state(static_cast<float>(bp35a1_->getChannelNumeric()));
    }
    if (pan_id_text_sensor_) {
        pan_id_text_sensor_->publish_state(bp35a1_->getPanId());
    }
    if (lqi_sensor_) {
        lqi_sensor_->publish_state(static_cast<float>(bp35a1_->getLQINumeric()));
    }
    if (pair_id_text_sensor_) {
        pair_id_text_sensor_->publish_state(bp35a1_->getPairId());
    }
    if (scan_mode_text_sensor_) {
        scan_mode_text_sensor_->publish_state(bp35a1_->getScanModeString());
    }
}

void BP35A1SmartMeterComponent::publish_meter_info_sensors_(const LowVoltageSmartElectricEnergyMeterClass &meter) {
    ESP_LOGI(TAG, "Publishing meter info sensors");

    if (installation_location_text_sensor_) {
        std::vector<uint8_t> location;
        if (meter.getInstallationLocation(&location)) {
            installation_location_text_sensor_->publish_state(bytes_to_hex(location));
        }
    }

    if (standard_version_information_text_sensor_) {
        uint32_t version;
        if (meter.getStandardVersionInformation(&version)) {
            char buf[12];
            snprintf(buf, sizeof(buf), "0x%08X", version);
            standard_version_information_text_sensor_->publish_state(buf);
        }
    }

    if (manufacturer_code_text_sensor_) {
        std::vector<uint8_t> code;
        if (meter.getManufacturerCode(&code)) {
            manufacturer_code_text_sensor_->publish_state(bytes_to_hex(code));
        }
    }

    if (get_property_map_text_sensor_) {
        std::vector<uint8_t> map;
        if (meter.getPropertyMap(&map)) {
            get_property_map_text_sensor_->publish_state(bytes_to_hex(map));
        }
    }
}

void BP35A1SmartMeterComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "BP35A1 Smart Meter");
    LOG_SENSOR("  ", "Power", power_sensor_);
    LOG_SENSOR("  ", "Current R", current_r_sensor_);
    LOG_SENSOR("  ", "Current T", current_t_sensor_);
    LOG_SENSOR("  ", "Energy", energy_sensor_);
    LOG_SENSOR("  ", "Energy Reverse", energy_reverse_sensor_);
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
    LOG_TEXT_SENSOR("  ", "Installation Location", installation_location_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Standard Version Information", standard_version_information_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Manufacturer Code", manufacturer_code_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Get Property Map", get_property_map_text_sensor_);
}

}  // namespace bp35a1_smartmeter
}  // namespace esphome
