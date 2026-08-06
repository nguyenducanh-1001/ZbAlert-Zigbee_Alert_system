#pragma once

#include "Zigbee.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

// định nghĩa ở pir_handler.h, dùng làm callback occupancy
void onPirReport(bool motion, uint8_t srcEndpoint, uint16_t shortAddr);

class ZigbeePirReceiver : public ZigbeeEP {
public:
  ZigbeePirReceiver(uint8_t endpoint) : ZigbeeEP(endpoint) {
    _device_id = ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID;
    _device = nullptr;
    _on_occupancy = nullptr;

    _cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_occupancy_sensing_cluster(
      _cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE
    );
    esp_zb_cluster_list_add_power_config_cluster(
      _cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE
    );

    _ep_config = {
      .endpoint = _endpoint,
      .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      .app_device_id = ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID,
      .app_device_version = 0,
    };
  }

  void onOccupancyChange(void (*callback)(bool, uint8_t, uint16_t)) {
    _on_occupancy = callback;
  }

  void onBatteryChange(void (*callback)(uint8_t percent, uint8_t srcEndpoint, uint16_t shortAddr)) {
    _on_battery = callback;
  }

private:
  zb_device_params_t *_device;
  unsigned long _devicePendingSince = 0;  // thời điểm _device được gán, dùng để phát hiện bind bị treo
  static const unsigned long BIND_PENDING_TIMEOUT_MS = 8000UL;
  void (*_on_occupancy)(bool motion, uint8_t srcEndpoint, uint16_t shortAddr);
  void (*_on_battery)(uint8_t percent, uint8_t srcEndpoint, uint16_t shortAddr) = nullptr;

  // Theo dõi bind Power Config riêng, chống bind trùng khi findCb() bị SDK
  // gọi lặp (bug đã biết) - nếu không sẽ bind 2 lần -> report bị nhân đôi.
  bool _powerConfigBound = false;
  bool _powerConfigPending = false;
  unsigned long _powerConfigPendingSince = 0;

  bool alreadyBound(const zb_device_params_t *device) {
    for (const auto &bound : _bound_devices) {
      if (bound == nullptr) {
        continue;
      }

      bool sameEndpoint = bound->endpoint == device->endpoint;
      bool sameShort = bound->short_addr == device->short_addr;
      bool sameLong = memcmp(bound->ieee_addr, device->ieee_addr, sizeof(esp_zb_ieee_addr_t)) == 0;

      if (sameEndpoint && (sameShort || sameLong)) {
        return true;
      }
    }

    return false;
  }

  void bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance == nullptr || instance->_device == nullptr) {
      return;
    }

    zb_device_params_t *sensor = instance->_device;
    instance->_device = nullptr;

    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
      if (!instance->alreadyBound(sensor)) {
        instance->_bound_devices.push_back(sensor);
        Serial.printf(
          "PIR bound: short=0x%04X endpoint=%u ieee=%s\n",
          sensor->short_addr,
          sensor->endpoint,
          Zigbee.formatIEEEAddress(sensor->ieee_addr)
        );
      } else {
        free(sensor);
      }

      instance->_is_bound = true;
      return;
    }

    Serial.printf("PIR bind failed, status=%u\n", zdo_status);
    free(sensor);
    instance->_is_bound = !instance->_bound_devices.empty();
  }

  static void bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance != nullptr) {
      instance->bindCb(zdo_status, user_ctx);
    }
  }

  // Bind cho Power Config cluster (báo % pin) - có guard chống bind trùng
  // giống hệt cơ chế bên occupancy, vì findCb() cũng có thể bị gọi lặp cho
  // cluster này.
  void bindPowerConfigCb(esp_zb_zdp_status_t zdo_status) {
    _powerConfigPending = false;

    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
      _powerConfigBound = true;
      Serial.println("Power Config (battery) bound thanh cong.");
    } else {
      Serial.printf("Power Config bind failed, status=%u\n", zdo_status);
    }
  }

  static void bindPowerConfigCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance != nullptr) {
      instance->bindPowerConfigCb(zdo_status);
    }
  }

  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance == nullptr) {
      return;
    }

    if (zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS) {
      return;
    }

    // 0xFFFF (broadcast short addr) va 255 (wildcard endpoint) khong phai dia
    // chi thiet bi that - loc ngay tai day de khong ton 1 lan bind request
    // chac chan fail (status=133 timeout). Truoc day chi loc o addOrUpdateDevice()
    // ben device_registry.h, tuc la sau khi da gui bind request roi.
    if (addr == 0xFFFF || endpoint == 0xFF) {
      return;
    }

    zb_device_params_t *sensor = (zb_device_params_t *)calloc(1, sizeof(zb_device_params_t));
    if (sensor == nullptr) {
      Serial.println("Cannot allocate PIR device record.");
      return;
    }

    sensor->endpoint = endpoint;
    sensor->short_addr = addr;
    esp_zb_ieee_address_by_short(sensor->short_addr, sensor->ieee_addr);

    if (instance->alreadyBound(sensor)) {
      free(sensor);
      return;
    }

    // esp-zigbee-sdk đôi lúc gọi callback match 2 lần cho cùng 1 thiết bị
    // (do broadcast response bị lặp ở tầng mạng). Nếu đang có 1 bind request
    // khác chưa xong (_device != nullptr), KHÔNG được ghi đè ngay - nếu không
    // bindCb() của request cũ sẽ đọc nhầm dữ liệu, tạo ra 1 "device ma" với
    // địa chỉ sai (0xFFFF/255).
    // NHƯNG: nếu bindCb() không bao giờ được gọi (request bị rớt/timeout mà
    // SDK không báo lỗi) thì _device sẽ kẹt mãi mãi -> phải có timeout để tự
    // giải phóng, không thì mọi lần bind sau đều bị chặn vĩnh viễn.
    if (instance->_device != nullptr) {
      unsigned long pendingFor = millis() - instance->_devicePendingSince;

      if (pendingFor < BIND_PENDING_TIMEOUT_MS) {
        Serial.printf(
          "Bo qua findCb: dang co bind request khac dang cho short=0x%04X (%lums).\n",
          instance->_device->short_addr,
          pendingFor
        );
        free(sensor);
        return;
      }

      Serial.printf(
        "Bind request cu (short=0x%04X) bi treo qua %lums, huy va thu lai.\n", instance->_device->short_addr, pendingFor
      );
      free(instance->_device);
      instance->_device = nullptr;
    }

    esp_zb_zdo_bind_req_param_t bindReq;
    memset(&bindReq, 0, sizeof(bindReq));
    bindReq.req_dst_addr = addr;
    memcpy(bindReq.src_address, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bindReq.src_endp = endpoint;
    bindReq.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING;
    bindReq.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    esp_zb_get_long_address(bindReq.dst_address_u.addr_long);
    bindReq.dst_endp = instance->_endpoint;

    instance->_device = sensor;
    instance->_devicePendingSince = millis();
    Serial.printf("Binding PIR sensor: short=0x%04X endpoint=%u\n", addr, endpoint);
    esp_zb_zdo_device_bind_req(&bindReq, ZigbeePirReceiver::bindCbWrapper, this);

    // Bind thêm cho Power Config (báo pin) trên cùng thiết bị/endpoint -
    // Zigbee bind theo từng cluster riêng nên cần 1 request bind_req khác.
    // Có guard chống trùng giống occupancy - nếu không, findCb() bị SDK gọi
    // lặp sẽ bind Power Config 2 lần -> báo pin bị nhân đôi mỗi lần report.
    bool skipPowerConfigBind = false;

    if (instance->_powerConfigBound) {
      skipPowerConfigBind = true;
    } else if (instance->_powerConfigPending) {
      unsigned long pcPendingFor = millis() - instance->_powerConfigPendingSince;
      if (pcPendingFor < BIND_PENDING_TIMEOUT_MS) {
        skipPowerConfigBind = true;
      } else {
        Serial.println("Power Config bind cu bi treo qua lau, huy va thu lai.");
        instance->_powerConfigPending = false;
      }
    }

    if (!skipPowerConfigBind) {
      esp_zb_zdo_bind_req_param_t batteryBindReq;
      memset(&batteryBindReq, 0, sizeof(batteryBindReq));
      batteryBindReq.req_dst_addr = addr;
      memcpy(batteryBindReq.src_address, sensor->ieee_addr, sizeof(esp_zb_ieee_addr_t));
      batteryBindReq.src_endp = endpoint;
      batteryBindReq.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG;
      batteryBindReq.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
      esp_zb_get_long_address(batteryBindReq.dst_address_u.addr_long);
      batteryBindReq.dst_endp = instance->_endpoint;

      instance->_powerConfigPending = true;
      instance->_powerConfigPendingSince = millis();
      esp_zb_zdo_device_bind_req(&batteryBindReq, ZigbeePirReceiver::bindPowerConfigCbWrapper, this);
    }
  }

  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance != nullptr) {
      instance->findCb(zdo_status, addr, endpoint, user_ctx);
    }
  }

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) override {
    uint16_t clusterList[] = {ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG};
    esp_zb_zdo_match_desc_req_param_t occupancyReq = {
      .dst_nwk_addr = cmd_req->dst_nwk_addr,
      .addr_of_interest = cmd_req->addr_of_interest,
      .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      .num_in_clusters = 2,
      .num_out_clusters = 0,
      .cluster_list = clusterList,
    };

    esp_zb_zdo_match_cluster(&occupancyReq, ZigbeePirReceiver::findCbWrapper, this);
  }

  void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) override {
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
      if (attribute->id != ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID || attribute->data.value == nullptr) {
        return;
      }

      // Chuẩn Zigbee: giá trị này tính theo đơn vị 0.5% (200 = 100%)
      uint8_t raw = *(uint8_t *)attribute->data.value;
      uint8_t percent = raw / 2;

      if (_on_battery != nullptr) {
        _on_battery(percent, src_endpoint, src_address.u.short_addr);
      }
      return;
    }

    if (cluster_id != ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING) {
      return;
    }

    if (attribute->id != ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID || attribute->data.value == nullptr) {
      return;
    }

    uint8_t occupancy = *(uint8_t *)attribute->data.value;
    bool motion = (occupancy & 0x01) != 0;

    if (_on_occupancy != nullptr) {
      _on_occupancy(motion, src_endpoint, src_address.u.short_addr);
    }
  }
};