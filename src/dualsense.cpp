// only for windows
// linux support later

#include <iostream>
#include <cstdint>

#include "engine.h"

#ifdef WIN32_
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

constexpr uint16_t DUALSENSE_VID = 0x054C;
constexpr uint16_t DUALSENSE_PID = 0x0CE6;

struct DS5Trigger {
    uint8_t type;
    uint8_t params[10];
};

#pragma pack(push, 1)
struct DS5OutputReport {
    uint8_t    reportId;    // 0       = 0x02
    uint16_t   flags;       // 1-2     bitfield: bit2=trigger_r, bit3=trigger_l, bit8=mic_led, bit10=lightbar, bit12=player_led
    uint8_t    rumbleR;     // 3
    uint8_t    rumbleL;     // 4
    uint8_t    unk3[4];     // 5-8
    uint8_t    micLed;      // 9       0=off, 1=on, 2=pulse
    uint8_t    unk9;        // 10
    DS5Trigger triggerR;    // 11-21
    DS5Trigger triggerL;    // 22-32
    uint8_t    unk28[11];   // 33-43
    uint8_t    playerLed;   // 44      5-bit, LSB=left
    uint8_t    lightbarR;   // 45
    uint8_t    lightbarG;   // 46
    uint8_t    lightbarB;   // 47
};
#pragma pack(pop)

static_assert(sizeof(DS5OutputReport) == 48, "Report must be 48 bytes");

// Flag bits
constexpr uint16_t FLAG_RUMBLE_EMULATION = 0x0007; // bits 0-2 must all be set to enable rumble emulation
constexpr uint16_t FLAG_TRIGGER_R = 0x0004;
constexpr uint16_t FLAG_TRIGGER_L = 0x0008;
constexpr uint16_t FLAG_MIC_LED = 0x0100;
constexpr uint16_t FLAG_LIGHTBAR = 0x0400;
constexpr uint16_t FLAG_PLAYER_LED = 0x1000;

HANDLE FindDualSense() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(ifData);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, i, &ifData); ++i) {
        DWORD needed = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, nullptr, 0, &needed, nullptr);

        auto* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(needed);
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devInfo, &ifData, detail, needed, nullptr, nullptr)) {
            free(detail); continue;
        }

        HANDLE h = CreateFile(detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        free(detail);

        if (h == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrs = { sizeof(attrs) };
        if (HidD_GetAttributes(h, &attrs)
            && attrs.VendorID == DUALSENSE_VID
            && attrs.ProductID == DUALSENSE_PID)
        {
            // Confirm it's the HID game controller interface (output report = 48 bytes)
            PHIDP_PREPARSED_DATA ppd;
            HIDP_CAPS caps;
            if (HidD_GetPreparsedData(h, &ppd)) {
                HidP_GetCaps(ppd, &caps);
                HidD_FreePreparsedData(ppd);
                if (caps.OutputReportByteLength == 48) {
                    SetupDiDestroyDeviceInfoList(devInfo);
                    return h;
                }
            }
        }
        CloseHandle(h);
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return INVALID_HANDLE_VALUE;
}

bool SetLightbarColor(HANDLE hDevice, uint8_t r, uint8_t g, uint8_t b) {
    DS5OutputReport report = {};
    report.reportId = 0x02;
    report.flags = FLAG_LIGHTBAR;
    report.lightbarR = r;
    report.lightbarG = g;
    report.lightbarB = b;

    DWORD written = 0;
    return WriteFile(hDevice, &report, sizeof(report), &written, nullptr)
        && written == sizeof(report);
}

void Engine::initDSRGB() {
    HANDLE h = FindDualSense();
    if (h == INVALID_HANDLE_VALUE) {
        dsPresent = false;
        return;
    }
    dsPresent = true;
    dsHID = h;
}

void Engine::dualsense_setLightbarColor(unsigned char R, unsigned char G, unsigned char B) {
    if (!dsPresent) return;
    SetLightbarColor(dsHID, R, G, B);
}

bool Engine::isDualSenseAttached() {
    return dsPresent;
}
#endif