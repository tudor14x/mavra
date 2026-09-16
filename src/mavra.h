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

#define MAVRA_VERSION "v0.1"

#define MAVRA_64_BIT_MODE_ARG "/x64"

// linker mode arg is the same thing but with all caps
#define MAVRA_64_BIT_MODE_LINKER_ARG "/X64"

#ifndef LINKER_MODE
#define LINKER_MODE 0
#endif

#define _DEBUG

extern FILE *errfile;
extern char userPath[PATH_LEN_MAX];

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

typedef struct MavraContext {
    WSDKInstall *wsdkInstalls;
    int wsdkInstallsLen;

    VSInstall *vsInstalls;
    int vsInstallsLen;

    MSVCInstalls *msvcInstalls;
    int msvcInstallsLen;

    int wsdk, vs, ms;
} MavraContext;

int getUserPath(char **dst);
int loadWSDKInstalls(MavraContext *ctx);
int getCommandCode(char *cmdBuf);
bool doesVswhereExist(char *out);
int loadVSInstalls(MavraContext *ctx);
int loadMSVCInstalls(MavraContext *ctx);
int createDataFile(FILE **dataFile, char *const dataFilePath, MavraContext *ctx);
int readDataFile(FILE **dataFile, char *const dataFilePath, MavraContext *ctx);
void freeMavraContext(MavraContext *ctx);
void listSDKs(MavraContext *ctx);

#endif // MAVRA_H