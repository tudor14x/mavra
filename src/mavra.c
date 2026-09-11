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

FILE *errfile;
char userPath[PATH_LEN_MAX] = {0};

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

int getWSDKInstalls(WSDKInstall **dst, int *dstLen)
{
    assert(dst);
    assert(dstLen);

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

    *dst = (WSDKInstall *)calloc(installCount, sizeof(WSDKInstall));
    if (!(*dst))
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
            free(*dst);
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
            free(*dst);
            RegCloseKey(key);
            return 9;
        }

        pathLenMax = PATH_LEN_MAX;

        status = RegGetValueA(key, verKeyName, "ProductVersion", RRF_RT_REG_SZ, NULL, install.version, &pathLenMax);

        if (status != ERROR_SUCCESS)
        {
            fprintf(errfile, "win32_error(%lu): failed to retrieve `ProductVersion` value of subkey `%s`.\n", status, verKeyName);
            free(*dst);
            RegCloseKey(key);
            return 10;
        }

        // hiccup caused by microsoft's inconsistency; the .0 is ommited in the registry key, but in the folder it is there.
        {
            char buf[sizeof(install.version)];
            sprintf(buf, "%s.0", install.version);
            memcpy(install.version, buf, sizeof(buf));
        }

        (*dst)[i] = install;
    }

    RegCloseKey(key);

    *dstLen = installCount;
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

int getVSInstallation(VSInstall **dst, int *dstLen)
{
    assert(dst);
    assert(dstLen);

    char vswhereBuf[PATH_LEN_MAX] = {0};
    bool vswhereExists = doesVswhereExist(vswhereBuf);

    if(!vswhereExists) {
        fprintf(errfile, "error: vswhere not found. place it next to the executable or make sure it's in the environment variables.\n");
        return 36;
    }

    FILE *in = _popen(vswhereBuf, "rt");
    if (!in)
    {
        fprintf(errfile, "c_error(%d): failed to run vswhere.\n", errno);
        return 12;
    }

    /* caller-managed */
    VSInstall *installs;

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

    installs = (VSInstall *)calloc(installCount, sizeof(VSInstall));
    if (!installs)
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for VSInstall array\n", errno);
        return 37;
    }

    in = _popen(vswhereBuf, "rt");
    if (!in)
    {
        fprintf(stderr, "c_error(%d): failed to call vswhere.exe\n", errno);
        free(installs);
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
            installs[count++] = install;
            memset(&install, 0, sizeof(VSInstall));
        }
        else if (strcmp(key, "displayName") == 0)
        {
            assert(count > 0);
            memcpy(installs[count - 1].displayName, value, strlen(value));
        }
        else if (strcmp(key, "installationPath") == 0)
        {
            assert(count > 0);
            memcpy(installs[count - 1].dir, value, strlen(value));
        }
        else if (strcmp(key, "installationVersion") == 0)
        {
            assert(count > 0);
            memcpy(installs[count - 1].version, value, strlen(value));
        }
    }
    _pclose(in);

    if (count == 0)
    {
        fprintf(errfile, "error: no Visual Studio installations found\n");
        free(installs);
        return 14;
    }

    *dst = installs;
    *dstLen = count;

    return 0;
}

int createDataFile(FILE **dataFile, char *const dataFilePath, WSDKInstall **wsdkInstalls, int *wsdkMaxLen, VSInstall **vsInstalls, int *vsMaxLen, MSVCInstalls **msvcInstalls, int *wsdk, int *vs, int *ms)
{
    assert(dataFilePath);
    assert(wsdkInstalls);
    assert(wsdkMaxLen);
    assert(vsInstalls);
    assert(vsMaxLen);
    assert(wsdk);
    assert(vs);
    assert(ms);

    int ec;
    ec = getWSDKInstalls(wsdkInstalls, wsdkMaxLen);
    if (ec != 0)
    {
        fprintf(errfile, "error: failed to retrieve Windows SDK installation(s).\n");
        return ec;
    }

    ec = getVSInstallation(vsInstalls, vsMaxLen);
    if (ec != 0)
    {
        fprintf(errfile, "error: failed to retrieve Visual Studio installation(s).\n");
        free(*wsdkInstalls);
        return ec;
    }

    // each vs install has at least one msvc install
    (*msvcInstalls) = (MSVCInstalls *)calloc(*vsMaxLen, sizeof(MSVCInstalls));
    if (!(*msvcInstalls))
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for MSVCInstalls array while writing to data file.\n", errno);
        free(*wsdkInstalls);
        free(*vsInstalls);
        return 15;
    }

    for (int i = 0; i < (*vsMaxLen); ++i)
    {
        char fileBuf[PATH_LEN_MAX] = {0};

        int msvcMaxLen = 0;

        // i don't care. i don't want to use realloc; i'll just repeat the code.
        {
            WIN32_FIND_DATAA fd;
            HANDLE hFind;
            {

                snprintf(fileBuf, sizeof(fileBuf), "%s\\VC\\Tools\\MSVC\\*", (*vsInstalls)[i].dir);

                hFind = FindFirstFileA(fileBuf, &fd);
                if (hFind == INVALID_HANDLE_VALUE)
                {
                    fprintf(errfile, "w32_error(%d): could not open directory `%s` for fetching MSVC version.\n", GetLastError(), fileBuf);

                    free(*wsdkInstalls);
                    free(*vsInstalls);

                    for (int j = 0; j < *vsMaxLen; ++j)
                    {
                        free((*msvcInstalls)[j].installs);
                    }

                    free(*msvcInstalls);

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
        (*msvcInstalls)[i].installs = (MSVCInstall *)calloc(msvcMaxLen, sizeof(MSVCInstall));
        if (!((*msvcInstalls)[i].installs))
        {
            fprintf(errfile, "c_error(%d): failed to allocate memory for MSVCInstall array for Visual Studio installation %d while writing to data file\n", errno, i);

            free(*wsdkInstalls);
            free(*vsInstalls);

            for (int i = 0; i < *vsMaxLen; ++i)
            {
                free((*msvcInstalls)[i].installs);
            }

            free(*msvcInstalls);
            return 17;
        }

        {
            WIN32_FIND_DATAA fd;
            HANDLE hFind;
            {

                snprintf(fileBuf, sizeof(fileBuf), "%s\\VC\\Tools\\MSVC\\*", (*vsInstalls)[i].dir);

                hFind = FindFirstFileA(fileBuf, &fd);
                if (hFind == INVALID_HANDLE_VALUE)
                {
                    fprintf(errfile, "error: could not open directory `%s` for fetching MSVC version\n", fileBuf);

                    free(*wsdkInstalls);
                    free(*vsInstalls);

                    for (int i = 0; i < *vsMaxLen; ++i)
                    {
                        free((*msvcInstalls)[i].installs);
                    }

                    free(*msvcInstalls);

                    return 18;
                }
            }

            do
            {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    strcmp(fd.cFileName, ".") != 0 &&
                    strcmp(fd.cFileName, "..") != 0)
                {
                    memcpy((*msvcInstalls)[i].installs[(*msvcInstalls)[i].installsMaxLen].version, fd.cFileName, strlen(fd.cFileName) + 1);
                    (*msvcInstalls)[i].installsMaxLen++;
                }
            } while (FindNextFileA(hFind, &fd));

            // interesting name choice
            FindClose(hFind);
        }
    }

    *dataFile = fopen(dataFilePath, "w");
    if (!(*dataFile))
    {
        fprintf(errfile, "c_error(%d): failed to open data file `%s` for writing.\n", errno, dataFilePath);
        free(*wsdkInstalls);
        free(*vsInstalls);

        for (int i = 0; i < *vsMaxLen; ++i)
        {
            free((*msvcInstalls)[i].installs);
        }

        free(*msvcInstalls);
        return 19;
    }

    fprintf(*dataFile, "[wsdk]\n");
    for (int i = 0; i < *wsdkMaxLen; ++i)
    {
        fprintf(*dataFile, "%d;%s;%s\n", i + 1, (*wsdkInstalls)[i].dir, (*wsdkInstalls)[i].version);
    }

    fprintf(*dataFile, "[vs]\n");
    for (int i = 0; i < *vsMaxLen; ++i)
    {
        fprintf(*dataFile, "%d;%d;%s;%s;%s\n", i + 1, (*msvcInstalls)[i].installsMaxLen, (*vsInstalls)[i].dir, (*vsInstalls)[i].displayName, (*vsInstalls)[i].version);
    }

    fprintf(*dataFile, "[msvc]\n");

    int sum = 0;

    for (int i = 0; i < *vsMaxLen; ++i)
    {
        for (int j = 0; j < (*msvcInstalls)[i].installsMaxLen; ++j)
        {
            int idx = sum + j + 1;
            fprintf(*dataFile, "%d;%d;%s\n", idx, j + 1, (*msvcInstalls)[i].installs[j].version);
        }
        sum += (*msvcInstalls)[i].installsMaxLen;
    }

    fprintf(*dataFile, "[settings]\n");
    fprintf(*dataFile, "wsdk=%d\nvs=%d\nms=%d", (*wsdk) + 1, (*vs) + 1, (*ms) + 1);

    fclose(*dataFile);

    return 0;
}

int readDataFile(FILE **dataFile, char *const dataFilePath, WSDKInstall **wsdkInstalls, int *wsdkMaxLen, VSInstall **vsInstalls, int *vsMaxLen, MSVCInstalls **msvcInstalls, int *wsdk, int *vs, int *ms)
{
    assert(dataFile);
    assert(dataFilePath);
    assert(wsdkInstalls);
    assert(wsdkMaxLen);
    assert(vsInstalls);
    assert(msvcInstalls);
    assert(vsMaxLen);
    assert(wsdk);
    assert(vs);
    assert(ms);

    if ((*wsdkInstalls))
    {
        free(*wsdkInstalls);
    }
    if ((*vsInstalls))
    {
        free(*vsInstalls);
    }

    if ((*msvcInstalls))
    {
        for (int i = 0; i < *vsMaxLen; ++i)
        {
            free((*msvcInstalls)[i].installs);
        }

        free(*msvcInstalls);
    }

    char lineBuf[1024] = {0};
    int mode = MODE_NULL, line = 1;

    *wsdkMaxLen = 0;
    *vsMaxLen = 0;

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

        char temp[64];
        memcpy(temp, lineBuf, offset);
        int index = atoi(temp) - 1;

        if (index < 0)
        {
            fprintf(errfile, "%s:%d: error: toolchain index is below 1.\n", dataFilePath, line);
            fclose(*dataFile);
            return 20;
        }

        switch (mode)
        {
        case MODE_WSDK:
        {
            (*wsdkMaxLen)++;
        }
        break;

        case MODE_VS:
        {
            (*vsMaxLen)++;
        }
        break;

        // number of msvc installation is sequal to vsMaxLen so it's pointless to count them
        case MODE_MSVC:
            break;

        default:
        {
            fprintf(errfile, "%s:%d: error: failed to retrieve toolchain index: file malformed.\n", dataFilePath, line);
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

    (*wsdkInstalls) = (WSDKInstall *)calloc(*wsdkMaxLen, sizeof(WSDKInstall));
    if (!(*wsdkInstalls))
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for WSDKInstall array while fetching data.\n", errno);
        fclose(*dataFile);
        return 23;
    }

    (*vsInstalls) = (VSInstall *)calloc(*vsMaxLen, sizeof(VSInstall));
    if (!(*vsInstalls))
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for VSInstall array while fetching data.\n", errno);
        fclose(*dataFile);
        free(*wsdkInstalls);
        return 24;
    }

    (*msvcInstalls) = (MSVCInstalls *)calloc(*vsMaxLen, sizeof(MSVCInstalls));
    if (!(*msvcInstalls))
    {
        fprintf(errfile, "c_error(%d): failed to allocate memory for MSVCInstalls array while fetching data.\n", errno);
        fclose(*dataFile);
        free(*wsdkInstalls);
        free(*vsInstalls);

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

#define CLEANUP()                          \
    free(*wsdkInstalls);                   \
    free(*vsInstalls);                     \
    for (int i = 0; i < *vsMaxLen; ++i)    \
    {                                      \
        free((*msvcInstalls)[i].installs); \
    }                                      \
    free(*msvcInstalls);                   \
    fclose(*dataFile)

#define PANIC(KEYNAME)                                                                     \
    if (end == lineLength)                                                                 \
    {                                                                                      \
        fprintf(errfile, "%s:%d: error: expected %s.\n", dataFilePath, line - 1, KEYNAME); \
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

                memcpy((*wsdkInstalls)[wsdkLen].dir, buf, strlen(buf) + 1);

                GET_NEXT_VALUE(";", "toolchain version");

                memcpy((*wsdkInstalls)[wsdkLen].version, buf, strlen(buf) + 1);

                wsdkLen++;
            }
            break;

            case MODE_VS:
            {
                GET_NEXT_VALUE(";", "toolchain index");
                GET_NEXT_VALUE(";", "MSVC install count");
                int val = atoi(buf);

                (*vsInstalls)[vsLen].msvcInstallCount = val;

                (*msvcInstalls)[vsLen].installsMaxLen = (*vsInstalls)[vsLen].msvcInstallCount;
                (*msvcInstalls)[vsLen].installs = (MSVCInstall *)calloc((*msvcInstalls)[vsLen].installsMaxLen, sizeof(MSVCInstall));
                if (!((*msvcInstalls)[vsLen].installs))
                {
                    fprintf(errfile, "c_error(%lu): failed to allocate memory for MSVCInstall array while reading data file.\n", errno);

                    CLEANUP();

                    return 35;
                }

                GET_NEXT_VALUE(";", "installation path");

                memcpy((*vsInstalls)[vsLen].dir, buf, strlen(buf) + 1);

                GET_NEXT_VALUE(";", "display name");

                memcpy((*vsInstalls)[vsLen].displayName, buf, strlen(buf) + 1);

                GET_NEXT_VALUE(";", "toolchain version");

                memcpy((*vsInstalls)[vsLen].version, buf, strlen(buf) + 1);

                vsLen++;
            }
            break;

            case MODE_SETTINGS:
            {
                GET_NEXT_VALUE("=", "'=' after variable name");

                // TODO: implement type checking
                if (strcmp(buf, "wsdk") == 0)
                {
                    int val = atoi(lineView);
                    if (val < 1 || val > *wsdkMaxLen)
                    {
                        fprintf(errfile, "%s:%d: error: toolchain index (%d) out of bounds. expected integer between 1 and number of elements available (%d).\n", dataFilePath, line, val, *wsdkMaxLen);

                        CLEANUP();

                        return 27;
                    }

                    *wsdk = val - 1;
                }
                else if (strcmp(buf, "vs") == 0)
                {
                    int val = atoi(lineView);

                    if (val < 1 || val > *vsMaxLen)
                    {
                        fprintf(errfile, "%s:%d: error: toolchain index (%d) out of bounds. expected integer between 1 and number of elements available (%d).\n", dataFilePath, line, val, *vsMaxLen);

                        CLEANUP();

                        return 28;
                    }

                    *vs = val - 1;
                }
                else if (strcmp(buf, "ms") == 0)
                {
                    int val = atoi(lineView);

                    int msvcMaxLen = 0;

                    for (int i = 0; i < *vsMaxLen; ++i)
                    {
                        msvcMaxLen += (*vsInstalls)[i].msvcInstallCount;
                    }

                    if (val < 1 || val > msvcMaxLen)
                    {
                        fprintf(errfile, "%s:%d: error: toolchain index (%d) out of bounds. expected integer between 1 and number of elements available (%d).\n", dataFilePath, line, val, *vsMaxLen);

                        CLEANUP();

                        return 29;
                    }

                    *ms = val - 1;
                }
                else
                {
                    fprintf(errfile, "%s:%d: error: unknown variable `%s`.\n", dataFilePath, line, buf);

                    CLEANUP();

                    return 30;
                }
            }
            break;

            case MODE_MSVC:
            {
                GET_NEXT_VALUE(";", "toolchain index");

                // sum of every bla bla bla until this one
                int toolIndex = atoi(buf);

                GET_NEXT_VALUE(";", "MSVC installation index");

                int installIndex = atoi(buf);

                GET_NEXT_VALUE(";", "MSVC installation version");

                int msvcIndex = 0;
                int remaining = toolIndex;

                for (int i = 0; i < *vsMaxLen; ++i)
                {
                    if (remaining <= (*msvcInstalls)[i].installsMaxLen)
                    {
                        msvcIndex = i;
                        break;
                    }

                    remaining -= (*msvcInstalls)[i].installsMaxLen;
                }

                memcpy((*msvcInstalls)[msvcIndex].installs[installIndex - 1].version, buf, strlen(buf) + 1);
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

void listSDKs(WSDKInstall *wsdkInstalls, int wsdkMaxLen, VSInstall *vsInstalls, int vsMaxLen, MSVCInstalls *msvcInstalls, int wsdk, int vs, int ms)
{
    assert(wsdkInstalls);
    assert(wsdkMaxLen);
    assert(vsInstalls);
    assert(vsMaxLen);
    assert(msvcInstalls);

    printf("Windows SDK installations:\n");
    for (int i = 0; i < wsdkMaxLen; ++i)
    {
        printf("[%d] -- %s -- %s\n", i + 1, wsdkInstalls[i].dir, wsdkInstalls[i].version);
    }
    printf("\n");

    printf("Visual Studio installations:\n");
    for (int i = 0; i < vsMaxLen; ++i)
    {
        printf("[%d] -- %s -- %s -- %s\n", i + 1, vsInstalls[i].dir, vsInstalls[i].displayName, vsInstalls[i].version);

        printf("\tMSVC installations:\n");
        for (int j = 0; j < vsInstalls[i].msvcInstallCount; ++j)
        {
            printf("\t[%d] -- %s\n", (i + j + 1), msvcInstalls[i].installs[j].version);
        }
    }
    printf("\n");
}

// case-senstiive
bool beginsWith(const char *str, const char *control)
{
    return strncmp(str, control, strlen(control)) == 0;
}

// not case-sensitive
bool beginsWithNCS(const char *str, const char *control)
{
    int ctrlen = strlen(control), strrlen = strlen(str);
    if (ctrlen > strrlen)
    {
        return false;
    }

    for (int i = 0; i < ctrlen; ++i)
    {
        if (isalpha(str[i]))
        {
            if (tolower(str[i]) != tolower(control[i]))
            {
                return false;
            }
        }
        else
        {
            if (control[i] != str[i])
            {
                return false;
            }
        }
    }

    return true;
}

void toLowerStr(char *str)
{
    for (int i = 0; i < strlen(str); ++i)
    {
        str[i] = tolower(str[i]);
    }
}

int occurence(const char *str, const char *substr)
{
    char *ptr = strstr(str, substr);
    if (!ptr)
    {
        return strlen(str);
    }
    return (int)(ptr - str);
}

int main(int argc, char **argv)
{
    errfile = stderr;
    assert(errfile);

    int exitCode = 0;

    // save user path
    {
        char *temp;
        int ec = getUserPath(&temp);
        if (ec != 0)
        {
            fprintf(errfile, "error(%d): failed to retrieve user profile directory.\n", ec);
            return ec;
        }
        memcpy(userPath, temp, strlen(temp) + 1);
        free(temp);
    }

    WSDKInstall *wsdkInstalls = NULL;
    VSInstall *vsInstalls = NULL;

    // not a mistake; MSVCInstalls is a struct comprised of a MSVCInstall array and a len variable
    MSVCInstalls *msvcInstalls = NULL;

    // msvcMaxLen would be redundant as it's always equal to vsMaxLen
    int wsdkMaxLen = 0, vsMaxLen = 0, wsdk = 0, vs = 0, ms = 0;

#define SAVE_DATA()                                                                                                                       \
    int ec = createDataFile(&dataFile, dataFilePath, &wsdkInstalls, &wsdkMaxLen, &vsInstalls, &vsMaxLen, &msvcInstalls, &wsdk, &vs, &ms); \
    if (ec != 0)                                                                                                                          \
    {                                                                                                                                     \
        fprintf(errfile, "error(%d): failed to write data file.\n", ec);                                                                  \
        return ec;                                                                                                                        \
    }

    char dataFilePath[PATH_LEN_MAX];
    (void)snprintf(dataFilePath, sizeof(dataFilePath), "%s\\%s", userPath, MAVRA_INSTALL_DATA_PATH);
    FILE *dataFile = fopen(dataFilePath, "r");

    if (!dataFile)
    {
        SAVE_DATA();
    }
    else
    {
        int ec = readDataFile(&dataFile, dataFilePath, &wsdkInstalls, &wsdkMaxLen, &vsInstalls, &vsMaxLen, &msvcInstalls, &wsdk, &vs, &ms);
        if (ec != 0)
        {
            fprintf(errfile, "error(%d): failed to read data file.\n", ec);
            return ec;
        }
    }

#if defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && MASM_ARCHITECTURE_AGNOSTIC_BUILD == 0
    int targetArchitecture = TARGET_MASM_ARCHITECTURE;
#elif defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1
    int targetArchitecture = 86;
#endif // defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && MASM_ARCHITECTURE_AGNOSTIC_BUILD == 0

    bool printHelp = false, changed = false;

#define CLEANUP()                       \
    free(wsdkInstalls);                 \
    free(vsInstalls);                   \
    for (int i = 0; i < vsMaxLen; ++i)  \
    {                                   \
        free(msvcInstalls[i].installs); \
    }                                   \
    free(msvcInstalls)

    for (int i = 1; i < argc; i++)
    {
        toLowerStr(argv[i]);
        if (strcmp(argv[i], "/listsdks") == 0)
        {
            listSDKs(wsdkInstalls, wsdkMaxLen, vsInstalls, vsMaxLen, msvcInstalls, wsdk, vs, ms);
            goto _cleanup;
        } else if(strcmp(argv[i], "/help") == 0) {
            printHelp = true;
        } else if(strcmp(argv[i], "/version") == 0) {
            printf("mavra %s\n", MAVRA_VERSION);
            goto _cleanup;
        } else if(strcmp(argv[i], "/getwinsdk") == 0) {
            printf("%s\n", wsdkInstalls[wsdk].version);
            goto _cleanup;
        } else if(strcmp(argv[i], "/getvs") == 0) {
            printf("%s -- %s\n", vsInstalls[vs].displayName, vsInstalls[vs].version);
            goto _cleanup;
        } else if(strcmp(argv[i], "/getmsvc") == 0) {
            printf("%s\n", msvcInstalls[vs].installs[ms].version);
            goto _cleanup;
        } else {
            if (beginsWith(argv[i], "/setwinsdk:"))
            {
                argv[i] += strlen("/setwinsdk:");
                int val = atoi(argv[i]);

                if (val < 1 || val > wsdkMaxLen)
                {
                    fprintf(errfile, "error: failed to set Windows SDK: value (%d) is out of bounds. expected integer between %d and number of elements available (%d).\n", val, 1, wsdkMaxLen);

                    CLEANUP();

                    return 32;
                }

                wsdk = val - 1;
                
                printf("selected Windows Kit %s.\n", wsdkInstalls[wsdk].version);
                
                changed = true;
            }
            else if (beginsWith(argv[i], "/setvs:"))
            {
                argv[i] += strlen("/setvs:");
                int val = atoi(argv[i]);

                if (val < 1 || val > vsMaxLen)
                {
                    fprintf(errfile, "error: failed to set Visual Studio version: value (%d) is out of bounds. expected integer between %d and number of elements available (%d).\n", val, 1, vsMaxLen);

                    CLEANUP();

                    return 33;
                }

                vs = val - 1;

                printf("selected %s -- %s.\n", vsInstalls[vs].displayName, vsInstalls[vs].version);

                changed = true;
            }
            else if (beginsWith(argv[i], "/setmsvc:"))
            {
                argv[i] += strlen("/setmsvc:");
                int val = atoi(argv[i]);

                int msvcMaxLen = 0;

                for (int i = 0; i < vsMaxLen; ++i)
                {
                    msvcMaxLen += vsInstalls[i].msvcInstallCount;
                }

                if (val < 1 || val > msvcMaxLen)
                {
                    fprintf(errfile, "error: failed to set MSVC version: value (%d) is out of bounds. expected integer between %d and number of elements available (%d).\n", val, 1, msvcMaxLen);

                    CLEANUP();

                    return 34;
                }

                int remaining = val - 1;

                for (int i = 0; i < vsMaxLen; ++i)
                {
                    if (remaining < vsInstalls[i].msvcInstallCount)
                    {
                        vs = i;
                        ms = remaining;

                        printf("selected MSVC %s from %s -- %s.\n",
                               msvcInstalls[vs].installs[ms].version,
                               vsInstalls[vs].displayName,
                               vsInstalls[vs].version);

                        break;
                    }

                    remaining -= vsInstalls[i].msvcInstallCount;
                }

                changed = true;
            }
        }
    }

    if(changed) {
        SAVE_DATA();
        goto _cleanup;
    }

    char *raw_args = GetCommandLineA();

    if (MASM_ARCHITECTURE_AGNOSTIC_BUILD)
    {
        for (int i = occurence(raw_args, "/x64"); i != strlen(raw_args); i = occurence(raw_args, "/x64"))
        {
            targetArchitecture = 64;
            memset(raw_args + i, ' ', 4);
        }
    }

    // strip the executable name from the arg and the two spaces
    size_t len = strlen(argv[0]) + 2;
    raw_args += len;
    char cmdBuf[8192] = {0}, cmdBuf2[sizeof(cmdBuf)] = {0};

#if defined(LINKER_MODE) && LINKER_MODE == 0
    snprintf(cmdBuf, sizeof(cmdBuf), "\"%s\\VC\\Tools\\MSVC\\%s\\bin\\Hostx%s\\x%d\\%s\"", vsInstalls[vs].dir, msvcInstalls[vs].installs[ms], XSTR(ARCHITECTURE), targetArchitecture, (targetArchitecture == 86) ? "ml" : "ml64");
#elif defined(LINKER_MODE) && LINKER_MODE == 1
    snprintf(cmdBuf, sizeof(cmdBuf), "\"%s\\VC\\Tools\\MSVC\\%s\\bin\\Hostx%s\\x%d\\link\"", vsInstalls[vs].dir, msvcInstalls[vs].installs[ms], XSTR(ARCHITECTURE), targetArchitecture);
#endif // definedE(LINKER_MODE) && LINKED_MODE == 0

    if (argc > 1)
    {
        char cLibPathBuf[PATH_LEN_MAX] = {0};
        // retrieve c runtime
        snprintf(cLibPathBuf, PATH_LEN_MAX, "%sLib\\%s\\", wsdkInstalls[wsdk].dir, wsdkInstalls[wsdk].version);

        char cwdBuf[PATH_LEN_MAX] = {0};
        GetCurrentDirectoryA((DWORD)PATH_LEN_MAX, cwdBuf);

#if defined(LINKER_MODE) && LINKER_MODE == 1
        snprintf(cmdBuf, PATH_LEN_MAX, "%s %s /LIBPATH:\"%sucrt\\x%d\" /LIBPATH:\"%sum\\x%d\" /LIBPATH:\"%s\\VC\\Tools\\MSVC\\%s\\lib\\x%d\"", cmdBuf, raw_args, cLibPathBuf, targetArchitecture, cLibPathBuf, targetArchitecture, vsInstalls[vs].dir, msvcInstalls[vs].installs[ms], targetArchitecture);
#else
        snprintf(cmdBuf2, PATH_LEN_MAX, "%s %s /link /LIBPATH:\"%sucrt\\x%d\" /link /LIBPATH:\"%sum\\x%d\" /link /LIBPATH:\"%s\\VC\\Tools\\MSVC\\%s\\lib\\x%d\"", cmdBuf, raw_args, cLibPathBuf, targetArchitecture, cLibPathBuf, targetArchitecture, vsInstalls[vs].dir, msvcInstalls[vs].installs[ms], targetArchitecture);
        memcpy(cmdBuf, cmdBuf2, sizeof(cmdBuf2));
#endif // defined(LINKER_MODE) && LINKER_MODE == 0
    }

#if defined(LINKER_MODE) && LINKER_MODE == 1
    if (argc == 1)
        printHelp = true;
#endif // defined(LINKER_MODE) && LINKER_MODE == 1

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};

    si.cb = sizeof(si);

    if (CreateProcessA(NULL, cmdBuf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD temp;
        GetExitCodeProcess(pi.hProcess, &temp);
        exitCode = (int)temp;

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // rogue file left out by masm
        FILE *f = fopen("mllink$.lnk", "r");
        if (f)
        {
            fclose(f);
            system("del mllink$.lnk");
        }

        if (printHelp)
        {
#if defined(LINKER_MODE) && LINKER_MODE == 0
            printf("/ListSDKs List all of the SDKs (Windows SDK, Visual Studio, MSVC)\n");
            printf("/SetWinSDK:<int> Sets the Windows SDK\n");
            printf("/SetVS:<int> Sets the Visual Studio toolchain\n");
            printf("/SetMSVC:<int> Sets the MSVC toolchain\n");
            printf("/GetWinSDK Gets the Windows SDK\n");
            printf("/GetVS Gets the Visual Studio toolchain\n");
            printf("/GetMSVC Gets the MSVC toolchain\n");
            printf("/version Prints the Mavra version\n");
#if defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && defined(ARCHITECTURE) && (MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1) && (ARCHITECTURE != 86)
            printf("/x64 Sets 64-bit mode\n");
#endif // defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && defined(ARCHITECTURE) && (MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1) && (ARCHITECTURE != 86)
#elif defined(LINKER_MODE) && LINKER_MODE == 1
            if (argc == 1)
            {
                printf("\t\b\b/LISTSDKS\n");
                printf("\t\b\b/SETWINSDK:int\n");
                printf("\t\b\b/SETVS:int\n");
                printf("\t\b\b/SETMSVC:int\n");
                printf("\t\b\b/GETWINSDK\n");
                printf("\t\b\b/GETVS\n");
                printf("\t\b\b/GETMSVC\n");
                printf("\t\b\b/VERSION\n");
#if defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && defined(ARCHITECTURE) && (MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1) && (ARCHITECTURE != 86)
                printf("\t\b\b/X64\n");
#endif // defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && defined(ARCHITECTURE) && (MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1) && (ARCHITECTURE != 86)
            }
#endif // defined(LINKER_MODE) && LINKER_MODE == 0
        }
    }

_cleanup:
    free(wsdkInstalls);
    free(vsInstalls);

    for (int i = 0; i < vsMaxLen; ++i)
    {
        free(msvcInstalls[i].installs);
    }

    free(msvcInstalls);
    return exitCode;
}