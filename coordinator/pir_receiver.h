#pragma once

#include "Zigbee.h"
#include "ha/esp_zigbee_ha_standard.h"

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

private:
  zb_device_params_t *_device;
  void (*_on_occupancy)(bool motion, uint8_t srcEndpoint, uint16_t shortAddr);

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

  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance == nullptr) {
      return;
    }

    if (zdo_status != ESP_ZB_ZDP_STATUS_SUCCESS) {
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
    Serial.printf("Binding PIR sensor: short=0x%04X endpoint=%u\n", addr, endpoint);
    esp_zb_zdo_device_bind_req(&bindReq, ZigbeePirReceiver::bindCbWrapper, this);
  }

  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    ZigbeePirReceiver *instance = static_cast<ZigbeePirReceiver *>(user_ctx);
    if (instance != nullptr) {
      instance->findCb(zdo_status, addr, endpoint, user_ctx);
    }
  }

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) override {
    uint16_t clusterList[] = {ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING};
    esp_zb_zdo_match_desc_req_param_t occupancyReq = {
      .dst_nwk_addr = cmd_req->dst_nwk_addr,
      .addr_of_interest = cmd_req->addr_of_interest,
      .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
      .num_in_clusters = 1,
      .num_out_clusters = 0,
      .cluster_list = clusterList,
    };

    esp_zb_zdo_match_cluster(&occupancyReq, ZigbeePirReceiver::findCbWrapper, this);
  }

  void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) override {
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
