#include <Arduino.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Select a Zigbee coordinator/router mode in Arduino IDE: Tools -> Zigbee mode"
#endif

#include "Zigbee.h"
#include "ha/esp_zigbee_ha_standard.h"
#include <list>

#define SWITCH_ENDPOINT 1
#define PIR_RECEIVER_ENDPOINT 2

#define PIR_NODE_ENDPOINT 10
#define ALARM_ENDPOINT 20

#define MAX_DEVICES 20
#define JOIN_OPEN_SECONDS 180
#define DEVICE_SCAN_INTERVAL_MS 5000UL
#define DEVICE_PRINT_INTERVAL_MS 30000UL
#define STATE_POLL_INTERVAL_MS 10000UL

#define ALARM_ACTION_NONE 0
#define ALARM_ACTION_ON 1
#define ALARM_ACTION_OFF 2

enum CoordinatorMode : uint8_t {
  MODE_AUTO_PIR = 1,
  MODE_MANUAL = 2,
};

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

ZigbeeSwitch zbSwitch(SWITCH_ENDPOINT);
ZigbeePirReceiver zbPirReceiver(PIR_RECEIVER_ENDPOINT);

struct DeviceRecord {
  uint16_t shortAddr;
  uint8_t endpoint;
  esp_zb_ieee_addr_t ieeeAddr;
  bool active;
  bool lastStateKnown;
  bool lastState;
  bool occupancyKnown;
  bool occupancy;
  unsigned long lastSeenMs;
};

DeviceRecord devices[MAX_DEVICES];
uint8_t deviceCount = 0;

CoordinatorMode currentMode = MODE_AUTO_PIR;
volatile uint8_t pendingAlarmAction = ALARM_ACTION_NONE;

bool lastPirKnown = false;
bool lastPirMotion = false;
uint16_t lastPirShortAddr = 0xFFFF;
uint8_t lastPirEndpoint = 0;
unsigned long lastPirReportMs = 0;

const char *modeName() {
  return currentMode == MODE_AUTO_PIR ? "mode 1: PIR -> alarm" : "mode 2: manual";
}

bool sameIeee(const esp_zb_ieee_addr_t left, const esp_zb_ieee_addr_t right) {
  return memcmp(left, right, sizeof(esp_zb_ieee_addr_t)) == 0;
}

int findDevice(const zb_device_params_t *dev) {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    bool sameShortAddress = devices[i].shortAddr == dev->short_addr;
    bool sameEndpoint = devices[i].endpoint == dev->endpoint;
    bool sameLongAddress = sameIeee(devices[i].ieeeAddr, dev->ieee_addr);

    if (sameEndpoint && (sameShortAddress || sameLongAddress)) {
      return i;
    }
  }

  return -1;
}

int findFreeSlot() {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      return i;
    }
  }

  return -1;
}

void saveDevice(uint8_t index, const zb_device_params_t *dev) {
  devices[index].shortAddr = dev->short_addr;
  devices[index].endpoint = dev->endpoint;
  memcpy(devices[index].ieeeAddr, dev->ieee_addr, sizeof(esp_zb_ieee_addr_t));
  devices[index].lastSeenMs = millis();
}

void addOrUpdateDevice(const zb_device_params_t *dev, bool seen[]) {
  int index = findDevice(dev);

  if (index < 0) {
    index = findFreeSlot();
    if (index < 0) {
      Serial.println("Device list is full. Cannot save new device.");
      return;
    }

    devices[index].active = true;
    devices[index].lastStateKnown = false;
    devices[index].lastState = false;
    devices[index].occupancyKnown = false;
    devices[index].occupancy = false;
    devices[index].lastSeenMs = millis();
    deviceCount++;

    Serial.printf(
      ">>> New bound device: slot=%d short=0x%04X endpoint=%u ieee=%s\n",
      index,
      dev->short_addr,
      dev->endpoint,
      Zigbee.formatIEEEAddress(dev->ieee_addr)
    );
  }

  saveDevice(index, dev);
  seen[index] = true;
}

void addBoundDevicesFrom(std::list<zb_device_params_t *> boundDevices, bool seen[]) {
  for (const auto &dev : boundDevices) {
    if (dev != nullptr) {
      addOrUpdateDevice(dev, seen);
    }
  }
}

void syncBoundDevices() {
  bool seen[MAX_DEVICES] = {false};

  addBoundDevicesFrom(zbSwitch.getBoundDevices(), seen);
  addBoundDevicesFrom(zbPirReceiver.getBoundDevices(), seen);

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].active && !seen[i]) {
      Serial.printf(
        "<<< Device removed: slot=%u short=0x%04X endpoint=%u ieee=%s\n",
        i,
        devices[i].shortAddr,
        devices[i].endpoint,
        Zigbee.formatIEEEAddress(devices[i].ieeeAddr)
      );
      devices[i].active = false;
      devices[i].lastStateKnown = false;
      devices[i].occupancyKnown = false;
      if (deviceCount > 0) {
        deviceCount--;
      }
    }
  }
}

void printHelp() {
  Serial.println();
  Serial.println("Coordinator commands:");
  Serial.println("  help       - show commands");
  Serial.println("  mode       - show current mode");
  Serial.println("  mode1/auto - PIR node controls alarm node");
  Serial.println("  mode2/man  - manual control only");
  Serial.println("  list       - print saved bound devices");
  Serial.println("  bound      - print raw Zigbee bound devices");
  Serial.println("  open       - open Zigbee network for pairing");
  Serial.println("  close      - close Zigbee network");
  Serial.println("  on         - turn alarm/light ON");
  Serial.println("  off        - turn alarm/light OFF");
  Serial.println("  toggle     - toggle alarm/light");
  Serial.println("  state      - read alarm/light state");
  Serial.println("  test       - turn ON for 2 seconds, then OFF");
  Serial.println("  reset      - Zigbee factory reset");
  Serial.println();
}

void printMode() {
  Serial.print("Current mode: ");
  Serial.println(modeName());

  if (lastPirKnown) {
    Serial.printf(
      "Last PIR: %s from short=0x%04X endpoint=%u, %lu second(s) ago\n",
      lastPirMotion ? "MOTION" : "CLEAR",
      lastPirShortAddr,
      lastPirEndpoint,
      (millis() - lastPirReportMs) / 1000UL
    );
  } else {
    Serial.println("Last PIR: none");
  }
}

void setMode(CoordinatorMode mode) {
  currentMode = mode;
  pendingAlarmAction = ALARM_ACTION_NONE;
  printMode();
}

void printRawBoundDevices() {
  Serial.println("Switch/alarm bound devices:");
  zbSwitch.printBoundDevices(Serial);
  Serial.println("PIR receiver bound devices:");
  zbPirReceiver.printBoundDevices(Serial);
}

void printDevices() {
  syncBoundDevices();

  Serial.println();
  Serial.println("===== Bound devices =====");
  Serial.print("Mode: ");
  Serial.println(modeName());

  if (deviceCount == 0) {
    Serial.println("No devices bound yet.");
  }

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    Serial.printf(
      "[%u] short=0x%04X endpoint=%u ieee=%s",
      i,
      devices[i].shortAddr,
      devices[i].endpoint,
      Zigbee.formatIEEEAddress(devices[i].ieeeAddr)
    );

    if (devices[i].endpoint == ALARM_ENDPOINT) {
      Serial.print(" alarm");
      if (devices[i].lastStateKnown) {
        Serial.printf(" state=%s", devices[i].lastState ? "ON" : "OFF");
      }
    }

    if (devices[i].endpoint == PIR_NODE_ENDPOINT) {
      Serial.print(" pir");
      if (devices[i].occupancyKnown) {
        Serial.printf(" motion=%s", devices[i].occupancy ? "YES" : "NO");
      }
    }

    if (devices[i].lastSeenMs > 0) {
      Serial.printf(" seen=%lus", (millis() - devices[i].lastSeenMs) / 1000UL);
    }

    Serial.println();
  }

  Serial.printf("Total: %u device(s)\n", deviceCount);
  Serial.println("=========================");
  Serial.println();
}

int firstAlarmDevice() {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].active && devices[i].endpoint == ALARM_ENDPOINT) {
      return i;
    }
  }

  return -1;
}

void printTarget(uint8_t index, const char *action) {
  Serial.printf(
    "%s -> slot=%u short=0x%04X endpoint=%u\n",
    action,
    index,
    devices[index].shortAddr,
    devices[index].endpoint
  );
}

void sendAlarmOn() {
  syncBoundDevices();
  int index = firstAlarmDevice();

  if (index >= 0) {
    printTarget(index, "Alarm ON");
    zbSwitch.lightOn(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    Serial.println("Alarm endpoint not found. Sending ON to all bound On/Off devices.");
    zbSwitch.lightOn();
  } else {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void sendAlarmOff() {
  syncBoundDevices();
  int index = firstAlarmDevice();

  if (index >= 0) {
    printTarget(index, "Alarm OFF");
    zbSwitch.lightOff(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    Serial.println("Alarm endpoint not found. Sending OFF to all bound On/Off devices.");
    zbSwitch.lightOff();
  } else {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void sendAlarmToggle() {
  syncBoundDevices();
  int index = firstAlarmDevice();

  if (index >= 0) {
    printTarget(index, "Alarm TOGGLE");
    zbSwitch.lightToggle(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    Serial.println("Alarm endpoint not found. Sending TOGGLE to all bound On/Off devices.");
    zbSwitch.lightToggle();
  } else {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void readAlarmState() {
  syncBoundDevices();
  int index = firstAlarmDevice();

  if (index >= 0) {
    printTarget(index, "Read state");
    zbSwitch.getLightState(devices[index].endpoint, devices[index].shortAddr);
    return;
  }

  if (zbSwitch.bound()) {
    Serial.println("Alarm endpoint not found. Reading state from all bound On/Off devices.");
    zbSwitch.getLightState();
  } else {
    Serial.println("No bound alarm/light yet. Pair alarm_node first.");
  }
}

void onLightStateChange(bool state) {
  Serial.printf("Light/alarm state report: %s\n", state ? "ON" : "OFF");

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (devices[i].active && devices[i].endpoint == ALARM_ENDPOINT) {
      devices[i].lastStateKnown = true;
      devices[i].lastState = state;
      devices[i].lastSeenMs = millis();
    }
  }
}

void markPirReport(uint8_t endpoint, uint16_t shortAddr, bool motion) {
  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].active) {
      continue;
    }

    if (devices[i].endpoint == endpoint && devices[i].shortAddr == shortAddr) {
      devices[i].occupancyKnown = true;
      devices[i].occupancy = motion;
      devices[i].lastSeenMs = millis();
    }
  }
}

void onPirReport(bool motion, uint8_t srcEndpoint, uint16_t shortAddr) {
  lastPirKnown = true;
  lastPirMotion = motion;
  lastPirShortAddr = shortAddr;
  lastPirEndpoint = srcEndpoint;
  lastPirReportMs = millis();

  markPirReport(srcEndpoint, shortAddr, motion);

  Serial.printf(
    "PIR report: %s from short=0x%04X endpoint=%u | %s\n",
    motion ? "MOTION" : "CLEAR",
    shortAddr,
    srcEndpoint,
    modeName()
  );

  if (currentMode == MODE_AUTO_PIR) {
    pendingAlarmAction = motion ? ALARM_ACTION_ON : ALARM_ACTION_OFF;
  }
}

void handlePendingAlarmAction() {
  uint8_t action = pendingAlarmAction;
  if (action == ALARM_ACTION_NONE) {
    return;
  }

  pendingAlarmAction = ALARM_ACTION_NONE;

  if (action == ALARM_ACTION_ON) {
    sendAlarmOn();
  } else if (action == ALARM_ACTION_OFF) {
    sendAlarmOff();
  }
}

void handleCommand(String command) {
  command.trim();
  command.toLowerCase();

  if (command.length() == 0) {
    return;
  }

  if (command == "help" || command == "?") {
    printHelp();
  } else if (command == "mode") {
    printMode();
  } else if (command == "mode1" || command == "auto") {
    setMode(MODE_AUTO_PIR);
  } else if (command == "mode2" || command == "manual" || command == "man") {
    setMode(MODE_MANUAL);
  } else if (command == "list") {
    printDevices();
  } else if (command == "bound") {
    printRawBoundDevices();
  } else if (command == "open") {
    Serial.printf("Opening Zigbee network for %u seconds...\n", JOIN_OPEN_SECONDS);
    Zigbee.openNetwork(JOIN_OPEN_SECONDS);
  } else if (command == "close") {
    Serial.println("Closing Zigbee network...");
    Zigbee.closeNetwork();
  } else if (command == "on") {
    sendAlarmOn();
  } else if (command == "off") {
    sendAlarmOff();
  } else if (command == "toggle") {
    sendAlarmToggle();
  } else if (command == "state") {
    readAlarmState();
  } else if (command == "test") {
    sendAlarmOn();
    delay(2000);
    sendAlarmOff();
  } else if (command == "reset") {
    Serial.println("Factory reset in 2 seconds...");
    delay(2000);
    Zigbee.factoryReset();
  } else {
    Serial.print("Unknown command: ");
    Serial.println(command);
    Serial.println("Type 'help' to see commands.");
  }
}

void handleSerial() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  handleCommand(command);
}

void setup() {
#ifdef RGB_BUILTIN
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
#endif

  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(500);

  memset(devices, 0, sizeof(devices));

  zbSwitch.setManufacturerAndModel("Espressif", "CoordinatorGateway");
  zbSwitch.allowMultipleBinding(true);
  zbSwitch.onLightStateChange(onLightStateChange);

  zbPirReceiver.setManufacturerAndModel("Espressif", "PirReceiver");
  zbPirReceiver.allowMultipleBinding(true);
  zbPirReceiver.onOccupancyChange(onPirReport);

  Zigbee.allowMultiEndpointBinding(true);

  Serial.println();
  Serial.println("Adding coordinator switch and PIR receiver endpoints...");
  Zigbee.addEndpoint(&zbSwitch);
  Zigbee.addEndpoint(&zbPirReceiver);

  Zigbee.setRebootOpenNetwork(JOIN_OPEN_SECONDS);

  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start. Restarting...");
    ESP.restart();
  }

  Serial.println("Coordinator started.");
  Serial.printf("Network is open for %u seconds after boot.\n", JOIN_OPEN_SECONDS);
  Serial.println("Mode 1: PIR node controls alarm node.");
  Serial.println("Mode 2: manual commands control alarm node.");
  printHelp();
  printMode();
}

void loop() {
  handleSerial();
  handlePendingAlarmAction();

  static unsigned long lastScan = 0;
  if (millis() - lastScan >= DEVICE_SCAN_INTERVAL_MS) {
    syncBoundDevices();
    lastScan = millis();
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= DEVICE_PRINT_INTERVAL_MS) {
    printDevices();
    lastPrint = millis();
  }

  static unsigned long lastPoll = 0;
  if (millis() - lastPoll >= STATE_POLL_INTERVAL_MS) {
    if (zbSwitch.bound()) {
      readAlarmState();
    }
    lastPoll = millis();
  }

  delay(20);
}
