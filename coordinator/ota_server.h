#pragma once

#include <Arduino.h>

#include "config.h"
#include "logging.h"
#include "uart_link.h"

#define OTA_BLOCK_SIZE          64
#define OTA_VERSION_LENGTH      16
#define OTA_ACK_TIMEOUT_MS      3000UL
#define OTA_MAX_RETRY           3

enum OTAPacketType : uint8_t
{
    OTA_PACKET_START = 0,
    OTA_PACKET_INFO,
    OTA_PACKET_DATA,
    OTA_PACKET_ACK,
    OTA_PACKET_FINISH,
    OTA_PACKET_ABORT
};

enum OTAState : uint8_t
{
    OTA_IDLE = 0,
    OTA_READY,
    OTA_STARTING,
    OTA_SENDING,
    OTA_WAIT_ACK,
    OTA_FINISHED,
    OTA_ABORTED,
    OTA_FAILED
};

struct OTAFirmwareInfo
{
    char version[OTA_VERSION_LENGTH];
    uint32_t firmwareSize;
    uint32_t crc32;
    uint16_t totalBlocks;
};

struct OTADataPacket
{
    uint16_t blockNumber;
    uint16_t length;
    uint8_t data[OTA_BLOCK_SIZE];
};

struct OTAServerStatus
{
    OTAState state;
    bool running;
    uint16_t currentBlock;
    uint8_t retryCount;
    uint8_t progress;
    unsigned long lastPacketTime;
};

extern OTAServerStatus otaStatus;
extern OTAFirmwareInfo otaFirmware;

void otaServerInit();
void otaServerLoop();

bool otaServerStart(const char *firmwarePath);
bool otaServerSendNextBlock();

void otaServerReceiveAck(uint16_t blockNumber);

void otaServerFinish();
void otaServerAbort();
void otaServerStop();
void otaServerReset();

bool otaServerIsBusy();

OTAState otaServerGetState();

uint8_t otaServerGetProgress();

OTAFirmwareInfo otaServerGetFirmwareInfo();