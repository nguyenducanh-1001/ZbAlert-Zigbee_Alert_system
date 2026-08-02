#pragma once

#include <Arduino.h>

#include "config.h"
#include "logging.h"

#define OTA_BLOCK_SIZE          64
#define OTA_VERSION_LENGTH      16
#define OTA_ACK_TIMEOUT_MS      3000UL
#define OTA_MAX_RETRY           3

enum OTAClientState : uint8_t
{
    OTA_CLIENT_IDLE = 0,

    OTA_CLIENT_READY,

    OTA_CLIENT_RECEIVING,

    OTA_CLIENT_WRITING,

    OTA_CLIENT_VERIFYING,

    OTA_CLIENT_FINISHED,

    OTA_CLIENT_FAILED
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

struct OTAClientStatus
{
    OTAClientState state;

    bool running;

    uint16_t currentBlock;

    uint8_t progress;

    unsigned long lastReceiveTime;
};

extern OTAClientStatus otaClientStatus;

extern OTAFirmwareInfo otaFirmware;

void otaClientInit();

void otaClientLoop();

bool otaClientStart(const OTAFirmwareInfo &info);

bool otaClientReceiveBlock(const OTADataPacket &packet);

void otaClientSendAck(uint16_t blockNumber);

bool otaClientVerify();

void otaClientFinish();

void otaClientAbort();

void otaClientReset();

bool otaClientIsBusy();

OTAClientState otaClientGetState();

uint8_t otaClientGetProgress();