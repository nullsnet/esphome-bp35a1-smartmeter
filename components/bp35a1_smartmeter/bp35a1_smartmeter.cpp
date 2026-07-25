#include "bp35a1_smartmeter.h"

namespace esphome {
namespace bp35a1_smartmeter {

static const char *const TAG = "bp35a1_smartmeter";

std::string BP35A1SmartMeterComponent::bytes_to_hex(const std::vector<uint8_t> &bytes) {
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
            ESP_LOGE(TAG, "Initialization timeout (%lus), restarting...", init_timeout_ms_ / 1000);
            esp_restart();
        } else {
            const uint32_t elapsed = (now - init_start_ms_) / 1000;
            ESP_LOGD(TAG, "Init progress: %lus / %lus, state=%u", elapsed, init_timeout_ms_ / 1000, (uint8_t)bp35a1_->getInitializeState());
        }
        const uint32_t panaFails = bp35a1_->getPanaFailCount();
        if (panaFails > last_pana_fail_count_) {
            ESP_LOGW(TAG, "B-route authentication failed %lu time(s) - check b_route_id and b_route_password", panaFails);
            last_pana_fail_count_ = panaFails;
        }
        bp35a1_->initializeLoop();
        return;
    }

    bp35a1_->communicationLoop(
        [this](const LowVoltageSmartElectricEnergyMeterClass &meter) {
            int32_t power;
            float currentR = 0.0f, currentT = 0.0f, energy = 0.0f;
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

            if (!info_sensors_published_) {
                if (info_batches_.empty()) {
                    std::vector<uint8_t> decoded;
                    if (meter.getPropertyMapDecoded(&decoded)) {
                        build_info_batches_(decoded);
                        ESP_LOGI(TAG, "GetPropertyMap decoded: %zu properties -> %zu batches", decoded.size(), info_batches_.size());
                    }
                } else {
                    publish_meter_info_sensors_(meter);
                    ESP_LOGI(TAG, "Info response received (step %u/%u)", info_request_step_, (uint)info_batches_.size());
                }
            }
        },
        BP35A1::CommunicationState::ready
    );

    if (!info_sensors_published_) {
        const auto commState = bp35a1_->getCommunicationState();

        if (commState != BP35A1::CommunicationState::ready) {
            if (info_batch_sent_ms_ != 0 && (now - info_batch_sent_ms_) >= info_batch_timeout_ms_) {
                ESP_LOGW(TAG, "Batch timeout (%lu ms) in state %u, queuing for retry",
                         info_batch_timeout_ms_, (uint)commState);
                if (!info_batches_.empty() && info_request_step_ > 0 && info_request_step_ <= info_batches_.size()) {
                    info_retry_batches_.push_back(info_batches_[info_request_step_ - 1]);
                }
                bp35a1_->resetCommunicationState();
                while (uart_adapter_->available()) uart_adapter_->read();
                info_batch_sent_ms_ = 0;
                info_batch_last_send_ms_ = now;
            }
        }

        if (bp35a1_->getCommunicationState() == BP35A1::CommunicationState::ready) {
            bool has_work = false;

            if (info_batches_.empty()) {
                ESP_LOGI(TAG, "Requesting GetPropertyMap...");
                bp35a1_->sendPropertyRequest(std::vector<EchonetLite::Property>{EchonetLite::Property::GetPropertyMap});
                has_work = true;
            } else if (info_request_step_ < info_batches_.size()) {
                if (info_batch_last_send_ms_ != 0 && (now - info_batch_last_send_ms_) < info_batch_delay_ms_) {
                    return;
                }
                const auto &batch = info_batches_[info_request_step_];
                std::string epc_list;
                for (size_t i = 0; i < batch.size(); i++) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "0x%02X", batch[i]);
                    if (i > 0) epc_list += ", ";
                    epc_list += buf;
                }
                ESP_LOGI(TAG, "Sending batch %u/%u: [%s]", info_request_step_ + 1, (uint)info_batches_.size(), epc_list.c_str());
                while (uart_adapter_->available()) uart_adapter_->read();
                bp35a1_->sendPropertyRequest(batch);
                info_batch_last_send_ms_ = now;
                info_batch_sent_ms_ = now;
                info_request_step_++;
                has_work = true;
            } else if (!info_retry_batches_.empty() && info_retry_step_ < info_retry_batches_.size()) {
                if (info_batch_last_send_ms_ != 0 && (now - info_batch_last_send_ms_) < info_batch_delay_ms_) {
                    return;
                }
                const auto &batch = info_retry_batches_[info_retry_step_];
                std::string epc_list;
                for (size_t i = 0; i < batch.size(); i++) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "0x%02X", batch[i]);
                    if (i > 0) epc_list += ", ";
                    epc_list += buf;
                }
                ESP_LOGI(TAG, "Retrying batch %u/%u: [%s]", info_retry_step_ + 1, (uint)info_retry_batches_.size(), epc_list.c_str());
                while (uart_adapter_->available()) uart_adapter_->read();
                bp35a1_->sendPropertyRequest(batch);
                info_batch_last_send_ms_ = now;
                info_batch_sent_ms_ = now;
                info_retry_step_++;
                has_work = true;
            } else if (info_batches_.empty() || info_request_step_ >= info_batches_.size()) {
                info_sensors_published_ = true;
                info_batch_sent_ms_ = 0;
                if (!info_retry_batches_.empty()) {
                    ESP_LOGI(TAG, "All info request batches sent (%zu retries)", info_retry_batches_.size());
                } else {
                    ESP_LOGI(TAG, "All info request batches sent");
                }
            }
        }
    }
}

void BP35A1SmartMeterComponent::update() {
    if (!bp35a1_) return;
    if (bp35a1_->getInitializeState() != BP35A1::InitializeState::readySmartMeter) {
        return;
    }
    if (!info_sensors_published_) {
        return;
    }

    ESP_LOGI(TAG, "Sending property request...");
    bp35a1_->sendPropertyRequest(std::vector<LowVoltageSmartElectricEnergyMeterClass::Property>{
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

void BP35A1SmartMeterComponent::publish_info_sensor_(text_sensor::TextSensor *sensor, uint8_t epc, const std::vector<uint8_t> &bytes) {
    if (!sensor || bytes.empty()) return;

    if (epc == 0x80 && bytes.size() >= 1) {  // OperationStatus
        sensor->publish_state(bytes[0] == 0x30 ? "ON" : "OFF");
    } else if (epc == 0x97 && bytes.size() >= 2) {  // CurrentTimeSetting
        char buf[9];
        snprintf(buf, sizeof(buf), (bytes.size() >= 3) ? "%02d:%02d:%02d" : "%02d:%02d",
                 bytes[0], bytes[1], (bytes.size() >= 3) ? bytes[2] : 0);
        sensor->publish_state(buf);
    } else if (epc == 0x98 && bytes.size() >= 4) {  // CurrentDateSetting
        uint16_t year = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
        char buf[20];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u", year, bytes[2], bytes[3]);
        sensor->publish_state(buf);
    } else if (epc == 0xE1 && bytes.size() >= 1) {  // CumulativeEnergyUnit
        static const char *units[] = {"1 kWh", "0.1 kWh", "0.01 kWh", "0.001 kWh", "0.0001 kWh",
                                      "?", "?", "?", "?", "?", "10 kWh", "100 kWh", "1000 kWh", "10000 kWh"};
        uint8_t idx = bytes[0];
        sensor->publish_state(idx < sizeof(units) / sizeof(units[0]) ? units[idx] : ("unknown (" + bytes_to_hex(bytes) + ")"));
    } else {
        std::string hex = bytes_to_hex(bytes);
        if (hex.length() > 128) hex = hex.substr(0, 125) + "...";
        sensor->publish_state(hex);
    }
}

void BP35A1SmartMeterComponent::publish_meter_info_sensors_(const LowVoltageSmartElectricEnergyMeterClass &meter) {
    ESP_LOGI(TAG, "Publishing meter info sensors");

    int published = 0;
    for (auto &[epc, sensor] : info_text_sensors_) {
        std::vector<uint8_t> bytes;
        if (meter.getVariableLengthPropertyData(epc, &bytes)) {
            publish_info_sensor_(sensor, epc, bytes);
            published++;
        }
    }

    ESP_LOGI(TAG, "Published %d meter info sensors (%zu configured)", published, info_text_sensors_.size());
}

void BP35A1SmartMeterComponent::build_info_batches_(const std::vector<uint8_t> &decoded_property_map) {
    // Set Property Map (setter) and periodic properties excluded from info requests
    static constexpr uint8_t exclude[] = {
        static_cast<uint8_t>(EchonetLite::Property::SetPropertyMap),
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyPositive),
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyNegative),
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::InstantaneousPower),
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::InstantaneousCurrents),
    };
    // Large payload properties get single-property batches
    static constexpr uint8_t large[] = {
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyHistoryPositive),
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyHistoryNegative),
        static_cast<uint8_t>(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyHistory2),
    };

    auto includes = [](uint8_t epc, const uint8_t *arr, size_t len) {
        for (size_t i = 0; i < len; i++) {
            if (epc == arr[i]) return true;
        }
        return false;
    };

    std::vector<uint8_t> normal;
    std::vector<uint8_t> large_props;
    normal.reserve(decoded_property_map.size());
    for (uint8_t epc : decoded_property_map) {
        if (includes(epc, exclude, sizeof(exclude))) continue;
        if (includes(epc, large, sizeof(large))) {
            large_props.push_back(epc);
        } else {
            normal.push_back(epc);
        }
    }

    info_batches_.clear();
    for (uint8_t epc : large_props) {
        info_batches_.emplace_back(std::vector<uint8_t>{epc});
    }
    const size_t batch_size = 4;
    for (size_t i = 0; i < normal.size(); i += batch_size) {
        info_batches_.emplace_back(normal.begin() + i, normal.begin() + std::min(i + batch_size, normal.size()));
    }

    ESP_LOGI(TAG, "Built %zu info batches from %zu properties (%zu large, %zu normal)",
             info_batches_.size(), normal.size() + large_props.size(), large_props.size(), normal.size());
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

    ESP_LOGCONFIG(TAG, "  Meter Info Sensors:");
    for (auto &[epc, sensor] : info_text_sensors_) {
        if (sensor) {
            ESP_LOGCONFIG(TAG, "    0x%02X: %s", epc, sensor->get_name().c_str());
        }
    }
}

}  // namespace bp35a1_smartmeter
}  // namespace esphome
