#include <switch.h>

#include "ntp.h"

#define INNER_HEAP_SIZE 0x300000

u32 __nx_applet_type = AppletType_None;

TimeServiceType __nx_time_service_type = TimeServiceType_System;

void __libnx_init_time(void);

void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));
    }

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc)) {
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        }
        setsysExit();
    }

    rc = nifmInitialize(NifmServiceType_User);
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));
    }

    rc = socketInitializeDefault();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));
    }

    rc = timeInitialize();
    if (R_FAILED(rc)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));
    }

    __libnx_init_time();
}

void __appExit(void) {
    timeExit();
    socketExit();
    nifmExit();
    smExit();
}

Result waitForConnection() {
    Result rc;

    for (int i = 0; i < 5; i++) {
        svcSleepThread(15 * 1e9);

        NifmInternetConnectionStatus status;
        rc = nifmGetInternetConnectionStatus(NULL, NULL, &status);
        if (R_SUCCEEDED(rc) && status == NifmInternetConnectionStatus_Connected) {
            return 0;
        }
    }

    return 1;
}

int main(int argc, char* argv[]) {
    Result rc;
    uint64_t ntpTime;
    uint64_t now;
    uint64_t lastUpdate = 0;

    while (true) {
        rc = waitForConnection();
        if (R_SUCCEEDED(rc)) {
            rc = getNtpTime(&ntpTime);
            if (R_SUCCEEDED(rc)) {
                timeSetCurrentTime(TimeType_NetworkSystemClock, ntpTime);
                lastUpdate = ntpTime;
            }
        }

        do {
            svcSleepThread(1 * 60 * 60 * 1e9);
            timeGetCurrentTime(TimeType_NetworkSystemClock, &now);
        } while (now - lastUpdate < 3 * 24 * 60 * 60);
    }

    return 0;
}
