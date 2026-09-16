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

int main(int argc, char **argv)
{
    errfile = stderr;
    assert(errfile);

    int exitCode = 0;

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

    MavraContext ctx = {0};

#define SAVE_DATA()                                                                                                                       \
    int ec = createDataFile(&dataFile, dataFilePath, &ctx);                                                                               \
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
        int ec = readDataFile(&dataFile, dataFilePath, &ctx);
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

    for (int i = 1; i < argc; i++)
    {
        strToLower(argv[i]);
        if (strcmp(argv[i], "/listsdks") == 0)
        {
            listSDKs(&ctx);
            goto _cleanup;
        } else if((strcmp(argv[i], "/help") == 0) || (strcmp(argv[i], "/?") == 0)) {
            printHelp = true;
        } else if(strcmp(argv[i], "/version") == 0) {
            printf("mavra %s\n", MAVRA_VERSION);
            goto _cleanup;
        } else if(strcmp(argv[i], "/getwinsdk") == 0) {
            printf("%s\n", ctx.wsdkInstalls[ctx.wsdk].version);
            goto _cleanup;
        } else if(strcmp(argv[i], "/getvs") == 0) {
            printf("%s -- %s\n", ctx.vsInstalls[ctx.vs].displayName, ctx.vsInstalls[ctx.vs].version);
            goto _cleanup;
        } else if(strcmp(argv[i], "/getmsvc") == 0) {
            printf("%s\n", ctx.msvcInstalls[ctx.vs].installs[ctx.ms].version);
            goto _cleanup;
        } else {
            if (strBeginsWith(argv[i], "/setwinsdk:", false))
            {
                argv[i] += strlen("/setwinsdk:");
                int val = atoi(argv[i]);

                if (val < 1 || val > ctx.wsdkInstallsLen)
                {
                    fprintf(errfile, "error: failed to set Windows SDK: value (%d) is out of bounds. expected integer between %d and number of elements available (%d).\n", val, 1, ctx.wsdkInstallsLen);

                    freeMavraContext(&ctx);

                    return 32;
                }

                ctx.wsdk = val - 1;
                
                printf("selected Windows Kit %s.\n", ctx.wsdkInstalls[ctx.wsdk].version);
                
                changed = true;
            }
            else if (strBeginsWith(argv[i], "/setvs:", false))
            {
                argv[i] += strlen("/setvs:");
                int val = atoi(argv[i]);

                if (val < 1 || val > ctx.vsInstallsLen)
                {
                    fprintf(errfile, "error: failed to set Visual Studio version: value (%d) is out of bounds. expected integer between %d and number of elements available (%d).\n", val, 1, ctx.vsInstallsLen);

                    freeMavraContext(&ctx);

                    return 33;
                }

                ctx.vs = val - 1;

                printf("selected %s -- %s.\n", ctx.vsInstalls[ctx.vs].displayName, ctx.vsInstalls[ctx.vs].version);

                changed = true;
            }
            else if (strBeginsWith(argv[i], "/setmsvc:", false))
            {
                argv[i] += strlen("/setmsvc:");
                int val = atoi(argv[i]);

                int msvcMaxLen = 0;

                for (int i = 0; i < ctx.vsInstallsLen; ++i)
                {
                    msvcMaxLen += ctx.vsInstalls[i].msvcInstallCount;
                }

                if (val < 1 || val > msvcMaxLen)
                {
                    fprintf(errfile, "error: failed to set MSVC version: value (%d) is out of bounds. expected integer between %d and number of elements available (%d).\n", val, 1, msvcMaxLen);

                    freeMavraContext(&ctx);

                    return 34;
                }

                int remaining = val - 1;

                for (int i = 0; i < ctx.vsInstallsLen; ++i)
                {
                    if (remaining < ctx.vsInstalls[i].msvcInstallCount)
                    {
                        ctx.vs = i;
                        ctx.ms = remaining;

                        printf("selected MSVC %s from %s -- %s.\n",
                               ctx.msvcInstalls[ctx.vs].installs[ctx.ms].version,
                               ctx.vsInstalls[ctx.vs].displayName,
                               ctx.vsInstalls[ctx.vs].version);

                        break;
                    }

                    remaining -= ctx.vsInstalls[i].msvcInstallCount;
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
        for (int i = strOccurence(raw_args, MAVRA_64_BIT_MODE_ARG); i != strlen(raw_args); i = strOccurence(raw_args, MAVRA_64_BIT_MODE_ARG))
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
    snprintf(cmdBuf, sizeof(cmdBuf), "\"%s\\VC\\Tools\\MSVC\\%s\\bin\\Hostx%s\\x%d\\%s\"", ctx.vsInstalls[ctx.vs].dir, ctx.msvcInstalls[ctx.vs].installs[ctx.ms], XSTR(ARCHITECTURE), targetArchitecture, (targetArchitecture == 86) ? "ml" : "ml64");
#elif defined(LINKER_MODE) && LINKER_MODE == 1
    snprintf(cmdBuf, sizeof(cmdBuf), "\"%s\\VC\\Tools\\MSVC\\%s\\bin\\Hostx%s\\x%d\\link\"", ctx.vsInstalls[ctx.vs].dir, ctx.msvcInstalls[ctx.vs].installs[ctx.ms], XSTR(ARCHITECTURE), targetArchitecture);
#endif // definedE(LINKER_MODE) && LINKED_MODE == 0

    if (argc > 1)
    {
        char cLibPathBuf[PATH_LEN_MAX] = {0};
        // retrieve c runtime
        snprintf(cLibPathBuf, PATH_LEN_MAX, "%sLib\\%s\\", ctx.wsdkInstalls[ctx.wsdk].dir, ctx.wsdkInstalls[ctx.wsdk].version);

        char cwdBuf[PATH_LEN_MAX] = {0};
        GetCurrentDirectoryA((DWORD)PATH_LEN_MAX, cwdBuf);

#if defined(LINKER_MODE) && LINKER_MODE == 1
        snprintf(cmdBuf, PATH_LEN_MAX, "%s %s /LIBPATH:\"%sucrt\\x%d\" /LIBPATH:\"%sum\\x%d\" /LIBPATH:\"%s\\VC\\Tools\\MSVC\\%s\\lib\\x%d\"", cmdBuf, raw_args, cLibPathBuf, targetArchitecture, cLibPathBuf, targetArchitecture, ctx.vsInstalls[ctx.vs].dir, ctx.msvcInstalls[ctx.vs].installs[ctx.ms], targetArchitecture);
#else
        snprintf(cmdBuf2, PATH_LEN_MAX, "%s %s /link /LIBPATH:\"%sucrt\\x%d\" /link /LIBPATH:\"%sum\\x%d\" /link /LIBPATH:\"%s\\VC\\Tools\\MSVC\\%s\\lib\\x%d\"", cmdBuf, raw_args, cLibPathBuf, targetArchitecture, cLibPathBuf, targetArchitecture, ctx.vsInstalls[ctx.vs].dir, ctx.msvcInstalls[ctx.vs].installs[ctx.ms], targetArchitecture);
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
            printf("/GetWinSDK Gets the Windows SDK version currently in use\n");
            printf("/GetVS Prints the Visual Studio version currently in use\n");
            printf("/GetMSVC Prints the MSVC version currently in use\n");
            printf("/version Prints the version of this Mavra binary\n");
#if defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && defined(ARCHITECTURE) && (MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1) && (ARCHITECTURE != 86)
            printf(MAVRA_64_BIT_MODE_ARG" Sets 64-bit mode\n");
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
                printf("\t\b\b"MAVRA_64_BIT_MODE_LINKER_ARG"\n");
#endif // defined(MASM_ARCHITECTURE_AGNOSTIC_BUILD) && defined(ARCHITECTURE) && (MASM_ARCHITECTURE_AGNOSTIC_BUILD == 1) && (ARCHITECTURE != 86)
            }
#endif // defined(LINKER_MODE) && LINKER_MODE == 0
        }
    }

_cleanup:
    freeMavraContext(&ctx);
    return exitCode;
}