#ifndef MAVRA_H
#define MAVRA_H

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

// preceded by C:\\Users\\[user]\\  

#define MAVRA_INSTALL_DATA_PATH  "mavra.dat"
#define WSDK_INSTALL_REGKEY_PATH "SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows"

#define PATH_LEN_MAX   1024
#define REGKEY_LEN_MAX PATH_LEN_MAX

#define MODE_NULL     0
#define MODE_WSDK     1
#define MODE_VS       2
#define MODE_SETTINGS 3
#define MODE_MSVC     4

#ifndef ARCHITECTURE
#define ARCHITECTURE 86
#endif // ARCHITECTURE

#define STR(x) #x
#define XSTR(x) STR(x)

#ifndef TARGET_MASM_ARCHITECTURE
#define TARGET_MASM_ARCHITECTURE 86
#endif // TARGET_MASM_ARCHITECTURE

#ifndef MASM_ARCHITECTURE_AGNOSTIC_BUILD
#define MASM_ARCHITECTURE_AGNOSTIC_BUILD 1
#endif // MASM_ARCHITECTURE_AGNOSTIC_BUILD

#define MAVRA_VERSION "v1.0"

#ifndef LINKER_MODE
#define LINKER_MODE 0
#endif

#define _DEBUG

typedef struct WSDKInstall {
    char dir[PATH_LEN_MAX];
    char version[64];
} WSDKInstall;

typedef struct MSVCInstall {
    char version[32];
} MSVCInstall;

typedef struct MSVCInstalls {
    MSVCInstall *installs;
    size_t       installsMaxLen;
} MSVCInstalls;

typedef struct VSInstall {
    char version[32];
    char displayName[256];
    char dir[PATH_LEN_MAX];
    int  msvcInstallCount;
} VSInstall;

#endif // MAVRA_H