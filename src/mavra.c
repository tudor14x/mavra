#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <processenv.h>
#include <assert.h>
#include <windows.h>
#include <userenv.h>
#include "mavra.h"
#include "string_utils.h"

FILE *errfile = NULL;
char userPath[PATH_LEN_MAX] = {0};

#define FREE_WSDK() { free(ctx->wsdkInstalls); ctx->wsdkInstallsLen = 0; }
#define FREE_VS() { free(ctx->vsInstalls); ctx->vsInstallsLen = 0; }
#define FREE_MSVC() { for(int i = 0; i < ctx->vsInstallsLen; ++i) { free(ctx->msvcInstalls[i].installs); } free(ctx->msvcInstalls); ctx->msvcInstallsLen = 0; }
#define FREE_ALL() FREE_WSDK(); FREE_VS(); FREE_MSVC()

int getUserPath(char **dst)
{
    assert(dst);
    HANDLE token;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        fprintf(errfile, "win32_error(%lu): could not open process token while fetching user path.\n", GetLastError());
        return 1;
    }

    unsigned long sz = PATH_LEN_MAX;
    char *buf = (char *)calloc(1, PATH_LEN_MAX);
    if (!buf)
    {
        fprintf(errfile, "c_error(%d): failed to allocate string buffer for user profile directory.\n", errno);
        CloseHandle(token);
        return 2;
    }
    if (!(GetUserProfileDirectoryA(token, buf, &sz)))
    {
        fprintf(errfile, "win32_error(%lu): could not retrieve user profile directory.\n", GetLastError());
        CloseHandle(token);
        free(buf);
        return 3;
    }

    {
        char *temp = (char *)realloc(buf, sz);
        if (!temp)
        {
            fprintf(errfile, "error(%d): failed to reallocate string buffer for user profile directory.\n", errno);
            CloseHandle(token);
            free(buf);
            return 4;
        }
        buf = temp;
    }

    CloseHandle(token);

    *dst = buf;
    return 0;
}

int loadWSDKInstalls(MavraContext *ctx)
{
    assert(ctx);

    HKEY key;
    LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, WSDK_INSTALL_REGKEY_PATH, 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &key);
    if (status != ERROR_SUCCESS)
    {
        fprintf(errfile, "win32_error(%lu): failed to open registry key of Windows SDK install root directory. check if you have Windows SDK installed.\n", status);
        return 4;
    }

    DWORD installCount;

    status = RegQueryInfoKeyA(key, NULL, NULL, NULL, &installCount, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    if (status != ERROR_SUCCESS)
    {
        fprintf(errfile, "win32_error(%lu): failed to query info about Windows SDK install root key.\n", status);
        RegCloseKey(key);
        return 5;
    }

    if (installCount == 0)
    {
        fprintf(errfile, "error: No Windows SDK installations found.\n");
        RegCloseKey(key);
        return 6;
    }

    ctx->wsdkInstalls = (WSDKInstall *)calloc(installCount, sizeof(WSDKInstall));
    if (!(ctx->wsdkInstalls))
    {
        fprintf(errfile, "c_error(%lu): failed to allocate memory for Windows SDK install data.\n", errno);
        RegCloseKey(key);
        return 7;
    }

    // version key name
    char verKeyName[REGKEY_LEN_MAX];
    for (DWORD i = 0; i < installCount; ++i)
    {
        status = RegEnumKey(key, i, verKeyName, REGKEY_LEN_MAX);
        if (status != ERROR_SUCCESS)
        {
            fprintf(errfile, "win32_error(%lu): failed to retrieve %lu-th subkey key of Windows SDK install registry key.\n", status);
            FREE_WSDK();
            RegCloseKey(key);
            return 8;
        }

        WSDKInstall install = {0};

        unsigned long pathLenMax = PATH_LEN_MAX;
        status = RegGetValueA(key, verKeyName, "InstallationFolder", RRF_RT_REG_SZ, NULL, install.dir, &pathLenMax);

        if (status != ERROR_SUCCESS)
        {
            printf("%lu\n", ERROR_MORE_DATA);
            fprintf(errfile, "win32_error(%lu): failed to retrieve `InstallationFolder` value of subkey `%s`.\n", status, verKeyName);
            FREE_WSDK();
            RegCloseKey(key);
            return 9;
        }

        pathLenMax = PATH_LEN_MAX;

        status = RegGetValueA(key, verKeyName, "ProductVersion", RRF_RT_REG_SZ, NULL, install.version, &pathLenMax);

        if (status != ERROR_SUCCESS)
        {
            fprintf(errfile, "win32_error(%lu): failed to retrieve `ProductVersion` value of subkey `%s`.\n", status, verKeyName);
            FREE_WSDK();
            RegCloseKey(key);
            return 10;
        }

        // hiccup caused by microsoft's inconsistency; the .0 is ommited in the registry key, but in the folder it is there.
        {
            char buf[sizeof(install.version)];
            sprintf(buf, "%s.0", install.version);
            memcpy(install.version, buf, sizeof(buf));
        }

        ctx->wsdkInstalls[i] = install;
    }

    RegCloseKey(key);

    ctx->wsdkInstallsLen = installCount;
    return 0;
}

int getCommandCode(char *cmdBuf)
{
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;

    DWORD exitCode = 1;

    HANDLE hNul = CreateFileA(
        "NUL",
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hNul == INVALID_HANDLE_VALUE)
        return false;

    si.hStdOutput = hNul;
    si.hStdError = hNul;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    if (CreateProcessA(
        NULL,
        cmdBuf,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hNul);

    return (int)exitCode;
}

bool doesVswhereExist(char *out)
{
    assert(out);

    char cwd[PATH_LEN_MAX] = {0};
    GetModuleFileNameA(NULL, cwd, PATH_LEN_MAX);
    {
        char *last = strrchr(cwd, '\\');
        if(last) {
            *last = 0;
        }
    }

    char fileBuf[PATH_LEN_MAX] = {0};
    sprintf(fileBuf, "%s\\vswhere", cwd);

    char vsBuf[] = "vswhere";

    int fileCode = getCommandCode(fileBuf);
    int vsCode = getCommandCode(vsBuf);

    if(fileCode == 0) {
        memcpy(out, fileBuf, strlen(fileBuf)+1);
    }
    if(vsCode == 0) {
        memcpy(out, vsBuf, strlen(vsBuf)+1);
    }

    return fileCode == 0 || vsCode == 0;
}

int loadVSInstalls(MavraContext *ctx)
{
    assert(ctx);

    char vswhereBuf[PATH_LEN_MAX] = {0};
    bool vswhereExists = doesVswhereExist(vswhereBuf);

    if(!vswhereExists) {
        fprintf(errfile, "error: vswhere not found. place it next to the executable or make sure the path to vswhere is in the environment variables.\n");
        return 36;
    }

    FILE *in = _popen(vswhereBuf, "rt");
    if (!in)
    {
        fprintf(errfile, "c_error(%d): failed to run vswhere.\n", errno);
        return 12;
    }

    DWORD installCount = 0;
    char line[2048];

    while (fgets(line, sizeof(line), in))
    {
        size_t sepIdx = strcspn(line, ":");
        if (sepIdx == strlen(line))
        {
            continue;
        }

        char key[512] = {0};
        strncpy(key, line, sepIdx);

        if (strcmp(key, "instanceId") == 0)
        {
            installCount += 1;
        }
    }

    _pclose(in);

    ctx->vsInstalls = (VSInstall *)calloc(installCount, sizeof(VSInstall));
    if (!ctx->vsInstalls)
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for VSInstall array.\n", errno);
        return 37;
    }

    in = _popen(vswhereBuf, "rt");
    if (!in)
    {
        fprintf(stderr, "c_error(%d): failed to call vswhere.\n", errno);
        FREE_VS();
        return 13;
    }

    VSInstall install = {0};
    DWORD count = 0;

    while (fgets(line, sizeof(line), in))
    {
        size_t sepIdx = strcspn(line, ":");
        if (sepIdx == strlen(line))
        {
            continue;
        }

        char key[512] = {0}, value[512] = {0};
        strncpy(key, line, sepIdx);
        strncpy(value, line + sepIdx + 2, strlen(line) - sepIdx);

        // remove newlines
        {
            size_t idx = strcspn(value, "\n");
            value[idx] = 0;
        }

        if (strcmp(key, "instanceId") == 0)
        {
            ctx->vsInstalls[count++] = install;
            memset(&install, 0, sizeof(VSInstall));
        }
        else if (strcmp(key, "displayName") == 0)
        {
            assert(count > 0);
            memcpy(ctx->vsInstalls[count - 1].displayName, value, strlen(value));
        }
        else if (strcmp(key, "installationPath") == 0)
        {
            assert(count > 0);
            memcpy(ctx->vsInstalls[count - 1].dir, value, strlen(value));
        }
        else if (strcmp(key, "installationVersion") == 0)
        {
            assert(count > 0);
            memcpy(ctx->vsInstalls[count - 1].version, value, strlen(value));
        }
    }
    _pclose(in);

    if (count == 0)
    {
        fprintf(errfile, "error: no Visual Studio installations found\n");
        FREE_VS();
        return 14;
    }

    ctx->vsInstallsLen = count;

    return 0;
}

int loadMSVCInstalls(MavraContext *ctx)
{
    assert(ctx);

    ctx->msvcInstalls = (MSVCInstalls *)calloc(ctx->vsInstallsLen, sizeof(MSVCInstalls));
    if (!ctx->msvcInstalls) {
        fprintf(errfile, "c_error(%d): failed to allocate memory for MSVCInstalls array while writing to data file.\n", errno);
        return 15;
    }

    for (int i = 0; i < ctx->vsInstallsLen; ++i) {
        char fileBuf[PATH_LEN_MAX] = {0};

        int msvcMaxLen = 0;

        // i don't care. i don't want to use realloc; i'll just repeat the code.
        {
            WIN32_FIND_DATAA fd;
            HANDLE hFind;
            {

                snprintf(fileBuf, sizeof(fileBuf), "%s\\VC\\Tools\\MSVC\\*", ctx->vsInstalls[i].dir);

                hFind = FindFirstFileA(fileBuf, &fd);
                if (hFind == INVALID_HANDLE_VALUE)
                {
                    fprintf(errfile, "w32_error(%d): could not open directory `%s` for fetching MSVC version.\n", GetLastError(), fileBuf);

                    freeMavraContext(ctx);

                    return 16;
                }
            }

            do
            {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    strcmp(fd.cFileName, ".") != 0 &&
                    strcmp(fd.cFileName, "..") != 0)
                {
                    msvcMaxLen++;
                }
            } while (FindNextFileA(hFind, &fd));

            // interesting name choice
            FindClose(hFind);
        }

        // allocate the memory for every msvc installation for the current i-th vs installation
        ctx->msvcInstalls[i].installs = (MSVCInstall *)calloc(msvcMaxLen, sizeof(MSVCInstall));
        if (!(ctx->msvcInstalls[i].installs))
        {
            fprintf(errfile, "c_error(%d): failed to allocate memory for MSVCInstall array for Visual Studio installation %d while writing to data file\n", errno, i);

            freeMavraContext(ctx);

            return 17;
        }

        {
            WIN32_FIND_DATAA fd;
            HANDLE hFind;
            {

                snprintf(fileBuf, sizeof(fileBuf), "%s\\VC\\Tools\\MSVC\\*", ctx->vsInstalls[i].dir);

                hFind = FindFirstFileA(fileBuf, &fd);
                if (hFind == INVALID_HANDLE_VALUE)
                {
                    fprintf(errfile, "error: could not open directory `%s` for fetching MSVC version\n", fileBuf);

                    freeMavraContext(ctx);

                    return 18;
                }
            }

            do
            {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    strcmp(fd.cFileName, ".") != 0 &&
                    strcmp(fd.cFileName, "..") != 0)
                {
                    memcpy(ctx->msvcInstalls[i].installs[ctx->msvcInstalls[i].installsMaxLen].version, fd.cFileName, strlen(fd.cFileName) + 1);
                    ctx->msvcInstalls[i].installsMaxLen++;
                }
            } while (FindNextFileA(hFind, &fd));

            // interesting name choice
            FindClose(hFind);
        }
    }
    return 0;
}

int createDataFile(FILE **dataFile, char *const dataFilePath, MavraContext *ctx)
{
    assert(dataFile);
    assert(dataFilePath);
    assert(ctx);

    int ec;
    ec = loadWSDKInstalls(ctx);
    if (ec != 0)
    {
        fprintf(errfile, "error: failed to retrieve Windows SDK installation(s).\n");
        return ec;
    }

    ec = loadVSInstalls(ctx);
    if (ec != 0)
    {
        fprintf(errfile, "error: failed to retrieve Visual Studio installation(s).\n");
        if(ctx->vsInstalls) FREE_VS();
        FREE_WSDK();
        return ec;
    }

    // each vs install has at least one msvc install
    ec = loadMSVCInstalls(ctx);
    if (ec != 0) {
        fprintf(errfile, "error: failed to retrieve MSVC installation(s).\n", errno);
        FREE_WSDK();
        FREE_VS();
        return ec;
    }

    *dataFile = fopen(dataFilePath, "w");
    if (!(*dataFile))
    {
        fprintf(errfile, "c_error(%d): failed to open data file `%s` for writing.\n", errno, dataFilePath);

        freeMavraContext(ctx);

        return 19;
    }

    fprintf(*dataFile, "[wsdk]\n");
    for (int i = 0; i < ctx->wsdkInstallsLen; ++i)
    {
        fprintf(*dataFile, "%d;%s;%s\n", i + 1, ctx->wsdkInstalls[i].dir, ctx->wsdkInstalls[i].version);
    }

    fprintf(*dataFile, "[vs]\n");
    for (int i = 0; i < ctx->vsInstallsLen; ++i)
    {
        fprintf(*dataFile, "%d;%d;%s;%s;%s\n", i + 1, ctx->msvcInstalls[i].installsMaxLen, ctx->vsInstalls[i].dir, ctx->vsInstalls[i].displayName, ctx->vsInstalls[i].version);
    }

    fprintf(*dataFile, "[msvc]\n");

    int sum = 0;

    for (int i = 0; i < ctx->vsInstallsLen; ++i)
    {
        for (int j = 0; j < ctx->msvcInstalls[i].installsMaxLen; ++j)
        {
            int idx = sum + j + 1;
            fprintf(*dataFile, "%d;%d;%s\n", idx, j + 1, ctx->msvcInstalls[i].installs[j].version);
        }
        sum += ctx->msvcInstalls[i].installsMaxLen;
    }

    fprintf(*dataFile, "[settings]\n");
    fprintf(*dataFile, "wsdk=%d\nvs=%d\nms=%d", ctx->wsdk + 1, ctx->vs + 1, ctx->ms + 1);

    fclose(*dataFile);

    return 0;
}

int readDataFile(FILE **dataFile, char *const dataFilePath, MavraContext *ctx)
{
    assert(dataFile);
    assert(dataFilePath);
    assert(ctx);

    if (ctx->wsdkInstalls)
    {
        FREE_WSDK();
    }
    if (ctx->vsInstalls)
    {
        FREE_VS();
    }

    if (ctx->msvcInstalls)
    {
        FREE_MSVC();
    }

    char lineBuf[1024] = {0};
    int mode = MODE_NULL, line = 1;

    // count all winsdk and vs installations to know exactly how much memory to allocate
    while (fgets(lineBuf, sizeof(lineBuf), *dataFile))
    {
        lineBuf[strcspn(lineBuf, "\n")] = 0;
        line++;
        short offset = 0;

        offset = strcspn(lineBuf, ";");

        if (offset == strlen(lineBuf))
        {
            if (strcmp(lineBuf, "[wsdk]") == 0)
            {
                mode = MODE_WSDK;
            }
            else if (strcmp(lineBuf, "[vs]") == 0)
            {
                mode = MODE_VS;
            }
            else if (strcmp(lineBuf, "[msvc]") == 0)
            {
                mode = MODE_MSVC;
            }
            continue;
        }

        char temp[64] = {0};
        memcpy(temp, lineBuf, offset);

        if(!strIsInteger(temp)) {
            fprintf(errfile, "error: expected integer while reading %s toolchain index, got `%s` instead.\n", (mode==MODE_MSVC)?"MSVC":((mode==MODE_VS)?"Visual Studio":"Windows SDK"), temp);
            return 39;
        }
        
        int index = atoi(temp) - 1;

        if (index < 0)
        {
            fprintf(errfile, "error: %s:%d: toolchain index is below 1.\n", dataFilePath, line);
            fclose(*dataFile);
            return 20;
        }

        switch (mode)
        {
        case MODE_WSDK:
        {
            ctx->wsdkInstallsLen++;
        }
        break;

        case MODE_VS:
        {
            ctx->vsInstallsLen++;
        }
        break;

        // number of msvc installation is sequal to ctx->vsInstallsLen so it's pointless to count them
        case MODE_MSVC:
            break;

        default:
        {
            fprintf(errfile, "error: %s:%d: failed to retrieve toolchain index: file malformed.\n", dataFilePath, line);
            fclose(*dataFile);
            return 21;
        }
        break;
        }
    }

    fclose(*dataFile);

    (*dataFile) = fopen(dataFilePath, "r");
    if (!(*dataFile))
    {
        fprintf(errfile, "c_error(%d): failed to open file %s for reading.\n", errno, dataFilePath);
        return 22;
    }

    ctx->wsdkInstalls = (WSDKInstall *)calloc(ctx->wsdkInstallsLen, sizeof(WSDKInstall));
    if (!ctx->wsdkInstalls)
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for WSDKInstall array while fetching data.\n", errno);
        fclose(*dataFile);
        return 23;
    }

    ctx->vsInstalls = (VSInstall *)calloc(ctx->vsInstallsLen, sizeof(VSInstall));
    if (!ctx->vsInstalls)
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for VSInstall array while fetching data.\n", errno);
        fclose(*dataFile);
        FREE_WSDK();
        return 24;
    }

    ctx->msvcInstalls = (MSVCInstalls *)calloc(ctx->vsInstallsLen, sizeof(MSVCInstalls));
    if (!ctx->msvcInstalls)
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for MSVCInstalls array while fetching data.\n", errno);
        fclose(*dataFile);
        FREE_WSDK();
        FREE_VS();
        return 25;
    }

    line = 1;
    mode = MODE_NULL;
    int wsdkLen = 0, vsLen = 0;

    while (fgets(lineBuf, sizeof(lineBuf), *dataFile))
    {
        lineBuf[strcspn(lineBuf, "\n")] = 0;
        line++;

        if (strcmp(lineBuf, "[wsdk]") == 0)
        {
            mode = MODE_WSDK;
        }
        else if (strcmp(lineBuf, "[vs]") == 0)
        {
            mode = MODE_VS;
        }
        else if (strcmp(lineBuf, "[settings]") == 0)
        {
            mode = MODE_SETTINGS;
        }
        else if (strcmp(lineBuf, "[msvc]") == 0)
        {
            mode = MODE_MSVC;
        }
        else
        {
            char buf[1024];
            char *lineView = (char *)lineBuf;
            const int lineLength = strlen(lineView);
            int begin = 0, end;

            switch (mode)
            {
            case MODE_NULL:
                break;

#define CLEANUP() freeMavraContext(ctx); fclose(*dataFile)
#define PANIC(KEYNAME)                                                                     \
    if (end == lineLength)                                                                 \
    {                                                                                      \
        fprintf(errfile, "error: %s:%d: expected %s.\n", dataFilePath, line - 1, KEYNAME); \
        CLEANUP();                                                                         \
        return 26;                                                                         \
    }
#define GET_NEXT_VALUE(SEP, KEYNAME) \
    end = strcspn(lineView, SEP);    \
    memset(buf, 0, sizeof(buf));     \
    strncpy(buf, lineView, end);     \
    lineView += end + 1;             \
    PANIC(KEYNAME)

            case MODE_WSDK:
            {
                // skip over this
                GET_NEXT_VALUE(";", "toolchain index");
                GET_NEXT_VALUE(";", "installation path");

                memcpy(ctx->wsdkInstalls[wsdkLen].dir, buf, strlen(buf) + 1);

                GET_NEXT_VALUE(";", "toolchain version");

                memcpy(ctx->wsdkInstalls[wsdkLen].version, buf, strlen(buf) + 1);

                wsdkLen++;
            }
            break;

            case MODE_VS:
            {
                GET_NEXT_VALUE(";", "toolchain index");
                GET_NEXT_VALUE(";", "MSVC install count");
                int val = atoi(buf);

                ctx->vsInstalls[vsLen].msvcInstallCount = val;

                ctx->msvcInstalls[vsLen].installsMaxLen = ctx->vsInstalls[vsLen].msvcInstallCount;
                ctx->msvcInstalls[vsLen].installs = (MSVCInstall *)calloc(ctx->msvcInstalls[vsLen].installsMaxLen, sizeof(MSVCInstall));
                if (!(ctx->msvcInstalls[vsLen].installs))
                {
                    fprintf(errfile, "c_error(%lu): failed to allocate memory for MSVCInstall array while reading data file.\n", errno);

                    CLEANUP();

                    return 35;
                }

                GET_NEXT_VALUE(";", "installation path");

                memcpy(ctx->vsInstalls[vsLen].dir, buf, strlen(buf) + 1);

                GET_NEXT_VALUE(";", "display name");

                memcpy(ctx->vsInstalls[vsLen].displayName, buf, strlen(buf) + 1);

                GET_NEXT_VALUE(";", "toolchain version");

                memcpy(ctx->vsInstalls[vsLen].version, buf, strlen(buf) + 1);

                vsLen++;
            }
            break;

            case MODE_SETTINGS:
            {
                GET_NEXT_VALUE("=", "'=' after variable name");

                if (strcmp(buf, "wsdk") == 0)
                {
                    if(!strIsInteger(lineView)) {
                        fprintf(errfile, "error: %s:%d: failed to parse Windows SDK toolchain index: expected integer after '=', got `%s` instead.\n", dataFilePath, line, lineView);
                        CLEANUP();
                        return 38;
                    }

                    int val = atoi(lineView);
                    if (val < 1 || val > ctx->wsdkInstallsLen)
                    {
                        fprintf(errfile, "error: %s:%d: failed to assign value to Windows SDK toolchain index: value (%d) out of bounds. expected integer between 1 and number of elements available (%d).\n", dataFilePath, line, val, ctx->wsdkInstallsLen);

                        CLEANUP();

                        return 27;
                    }

                    ctx->wsdk = val - 1;
                }
                else if (strcmp(buf, "vs") == 0)
                {
                    if(!strIsInteger(lineView)) {
                        fprintf(errfile, "error: failed to parse Visual Studio toolchain index: expected integer after '=', got `%s` instead.\n", lineView);
                        CLEANUP();
                        return 38;
                    }

                    int val = atoi(lineView);

                    if (val < 1 || val > ctx->vsInstallsLen)
                    {
                        fprintf(errfile, "error: %s:%d: failed to assign value to Visual Studio toolchain index: value (%d) out of bounds. expected integer between 1 and number of elements available (%d).\n", dataFilePath, line, val, ctx->vsInstallsLen);

                        CLEANUP();

                        return 28;
                    }

                    ctx->vs = val - 1;
                }
                else if (strcmp(buf, "ms") == 0)
                {
                    if(!strIsInteger(lineView)) {
                        fprintf(errfile, "error: %s:%d: error: failed to parse MSVC toolchain index: expected integer after '=', got `%s` instead.\n", dataFilePath, line, lineView);
                        CLEANUP();
                        return 38;
                    }

                    int val = atoi(lineView);

                    int msvcMaxLen = 0;

                    for (int i = 0; i < ctx->vsInstallsLen; ++i)
                    {
                        msvcMaxLen += ctx->vsInstalls[i].msvcInstallCount;
                    }

                    if (val < 1 || val > msvcMaxLen)
                    {
                        fprintf(errfile, "error: %s:%d: failed to assign value to MSVC toolchain index: value (%d) out of bounds. expected integer between 1 and number of elements available (%d).\n", dataFilePath, line, val, ctx->vsInstallsLen);

                        CLEANUP();

                        return 29;
                    }

                    ctx->ms = val - 1;
                }
                else
                {
                    fprintf(errfile, "error: %s:%d: unknown variable `%s`.\n", dataFilePath, line, buf);

                    CLEANUP();

                    return 30;
                }
            }
            break;

            case MODE_MSVC:
            {
                GET_NEXT_VALUE(";", "toolchain index");

                // sum of every bla bla bla until this one

                if(!strIsInteger(buf)) {
                    fprintf(errfile, "error: failed to parse MSVC installation: expected integer for toolIndex argument, got `%s` instead.\n", lineView);
                    CLEANUP();
                     return 38;
                }

                int toolIndex = atoi(buf);

                GET_NEXT_VALUE(";", "MSVC installation index");

                if(!strIsInteger(buf)) {
                    fprintf(errfile, "error: failed to parse MSVC installation: expected integer for toolIndex argument, got `%s` instead.\n", lineView);
                    CLEANUP();
                     return 38;
                }

                int installIndex = atoi(buf);

                GET_NEXT_VALUE(";", "MSVC installation version");

                int msvcIndex = 0;
                int remaining = toolIndex;

                for (int i = 0; i < ctx->vsInstallsLen; ++i)
                {
                    if (remaining <= ctx->msvcInstalls[i].installsMaxLen)
                    {
                        msvcIndex = i;
                        break;
                    }

                    remaining -= ctx->msvcInstalls[i].installsMaxLen;
                }

                memcpy(ctx->msvcInstalls[msvcIndex].installs[installIndex - 1].version, buf, strlen(buf) + 1);
            }
            break;

            default:
            {
                CLEANUP();
                assert(false && "UNREACHABLE");
                return 31;
            }
            break;
            }
        }
    }

    fclose(*dataFile);

#undef PANIC
#undef GET_NEXT_VALUE
#undef CLEANUP

    return 0;
}

void freeMavraContext(MavraContext *ctx)
{
    assert(ctx);
    FREE_ALL();
}

void listSDKs(MavraContext *ctx)
{
    assert(ctx);

    printf("Windows SDK installations:\n");
    for (int i = 0; i < ctx->wsdkInstallsLen; ++i)
    {
        printf("[%d] -- %s -- %s\n", i + 1, ctx->wsdkInstalls[i].dir, ctx->wsdkInstalls[i].version);
    }
    printf("\n");

    printf("Visual Studio installations:\n");
    for (int i = 0; i < ctx->vsInstallsLen; ++i)
    {
        printf("[%d] -- %s -- %s -- %s\n", i + 1, ctx->vsInstalls[i].dir, ctx->vsInstalls[i].displayName, ctx->vsInstalls[i].version);

        printf("\tMSVC installations:\n");
        for (int j = 0; j < ctx->vsInstalls[i].msvcInstallCount; ++j)
        {
            printf("\t[%d] -- %s\n", (i + j + 1), ctx->msvcInstalls[i].installs[j].version);
        }
    }
    printf("\n");
}