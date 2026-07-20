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
                ESP_LOGW(TAG, "Batch timeout (%ums) in state %u, queuing for retry",
                         info_batch_timeout_ms_, (uint)commState);
                if (!info_batches_.empty() && info_request_step_ > 0 && info_request_step_ <= info_batches_.size()) {
                    info_retry_batches_.push_back(info_batches_[info_request_step_ - 1]);
                }
                bp35a1_->resetCommunicationState();
                info_batch_sent_ms_ = 0;
                info_batch_last_send_ms_ = now;
                info_request_step_++;
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

void BP35A1SmartMeterComponent::publish_meter_info_sensors_(const LowVoltageSmartElectricEnergyMeterClass &meter) {
    ESP_LOGI(TAG, "Publishing meter info sensors");

    int published = 0;
    auto publish_hex = [&published](text_sensor::TextSensor *sensor, const std::string &name, const std::vector<uint8_t> &bytes) {
        if (sensor && !bytes.empty()) {
            std::string hex = bytes_to_hex(bytes);
            if (hex.length() > 128) {
                hex = hex.substr(0, 125) + "...";
            }
            sensor->publish_state(hex);
            published++;
            ESP_LOGI(TAG, "  %s: %s (%zu bytes)", name.c_str(), hex.c_str(), bytes.size());
        } else if (sensor) {
            ESP_LOGW(TAG, "  %s: no data", name.c_str());
        }
    };

    std::vector<uint8_t> bytes;

    // Base class properties (0x80-0x9F)
    if (meter.getVariableLengthPropertyData(EchonetLite::Property::OperationStatus, &bytes))
        publish_hex(operation_status_text_sensor_, "OperationStatus", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::InstallationLocation, &bytes))
        publish_hex(installation_location_text_sensor_, "InstallationLocation", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::StandardVersionInformation, &bytes))
        publish_hex(standard_version_information_text_sensor_, "StandardVersionInformation", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::FaultStatus, &bytes))
        publish_hex(fault_status_text_sensor_, "FaultStatus", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::ManufacturerCode, &bytes))
        publish_hex(manufacturer_code_text_sensor_, "ManufacturerCode", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::ProductionNumber, &bytes))
        publish_hex(production_number_text_sensor_, "ProductionNumber", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::CurrentTimeSetting, &bytes) && bytes.size() >= 2) {
        char buf[9];
        if (bytes.size() >= 3)
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", bytes[0], bytes[1], bytes[2]);
        else
            snprintf(buf, sizeof(buf), "%02d:%02d", bytes[0], bytes[1]);
        current_time_setting_text_sensor_->publish_state(buf);
        ESP_LOGD(TAG, "CurrentTimeSetting: %s", buf);
    } else {
        ESP_LOGD(TAG, "CurrentTimeSetting: not available (bytes=%zu)", bytes.size());
    }

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::CurrentDateSetting, &bytes) && bytes.size() >= 4) {
        uint16_t year = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
        char buf[11];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, bytes[2], bytes[3]);
        current_date_setting_text_sensor_->publish_state(buf);
        ESP_LOGD(TAG, "CurrentDateSetting: %s", buf);
    }

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::StatusChangeAnnouncementPropertyMap, &bytes))
        publish_hex(status_change_announcement_property_map_text_sensor_, "StatusChangeAnnouncementPropertyMap", bytes);

    if (meter.getVariableLengthPropertyData(EchonetLite::Property::GetPropertyMap, &bytes))
        publish_hex(get_property_map_text_sensor_, "GetPropertyMap", bytes);

    // Meter-specific properties (0xC0-0xEF)
    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::Coefficient, &bytes))
        publish_hex(coefficient_text_sensor_, "Coefficient", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeAmountEnergyEffectiveDigits, &bytes))
        publish_hex(cumulative_energy_effective_digits_text_sensor_, "CumulativeAmountEnergyEffectiveDigits", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyUnit, &bytes))
        publish_hex(cumulative_energy_unit_text_sensor_, "CumulativeEnergyUnit", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyHistoryPositive, &bytes))
        publish_hex(cumulative_energy_history_positive_text_sensor_, "CumulativeEnergyHistoryPositive", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyHistoryNegative, &bytes))
        publish_hex(cumulative_energy_history_negative_text_sensor_, "CumulativeEnergyHistoryNegative", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::DateOfCollectCumulativeEnergyHistory, &bytes))
        publish_hex(date_of_collect_cumulative_energy_history_text_sensor_, "DateOfCollectCumulativeEnergyHistory", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::FixedCumulativeEnergyPositive, &bytes))
        publish_hex(fixed_cumulative_energy_positive_text_sensor_, "FixedCumulativeEnergyPositive", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::FixedCumulativeEnergyNegative, &bytes))
        publish_hex(fixed_cumulative_energy_negative_text_sensor_, "FixedCumulativeEnergyNegative", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::CumulativeEnergyHistory2, &bytes))
        publish_hex(cumulative_energy_history2_text_sensor_, "CumulativeEnergyHistory2", bytes);

    if (meter.getVariableLengthPropertyData(LowVoltageSmartElectricEnergyMeterClass::Property::DateOfCollectCumulativeEnergyHistory2, &bytes))
        publish_hex(date_of_collect_cumulative_energy_history2_text_sensor_, "DateOfCollectCumulativeEnergyHistory2", bytes);

    ESP_LOGI(TAG, "Published %d meter info sensors (20 total properties checked)", published);
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
    LOG_TEXT_SENSOR("  ", "Installation Location", installation_location_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Standard Version Information", standard_version_information_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Fault Status", fault_status_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Manufacturer Code", manufacturer_code_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Production Number", production_number_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Get Property Map", get_property_map_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Operation Status", operation_status_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Current Time Setting", current_time_setting_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Current Date Setting", current_date_setting_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Status Change Announcement PM", status_change_announcement_property_map_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Coefficient", coefficient_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Cumulative Energy Effective Digits", cumulative_energy_effective_digits_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Cumulative Energy Unit", cumulative_energy_unit_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Cumulative Energy History Positive", cumulative_energy_history_positive_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Cumulative Energy History Negative", cumulative_energy_history_negative_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Date of Collect Cumulative Energy History", date_of_collect_cumulative_energy_history_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Fixed Cumulative Energy Positive", fixed_cumulative_energy_positive_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Fixed Cumulative Energy Negative", fixed_cumulative_energy_negative_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Cumulative Energy History2", cumulative_energy_history2_text_sensor_);
    LOG_TEXT_SENSOR("  ", "Date of Collect Cumulative Energy History2", date_of_collect_cumulative_energy_history2_text_sensor_);
}

}  // namespace bp35a1_smartmeter
}  // namespace esphome
