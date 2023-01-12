#pragma once

// until we can get rid of the c-code
#ifdef __cplusplus
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <thread>

// Convenience MACROS
#define PI_SLEEP_SEC(secs)  std::this_thread::sleep_for(std::chrono::seconds(secs));
#define PI_SLEEP_MSEC(ms)   std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#else
#include <stdlib.h>
#include <stdio.h>
#endif

#include <stdint.h>
#include <string.h>
#include <string>
#include "filesystem.hpp"

#ifdef WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h> // for _kbhit
#include <direct.h>
#include <WtsApi32.h>
// Cross platform loading of plugins
#define FAR far
typedef HINSTANCE PI_INSTANCE;
#define PI_LOAD_DYNAMIC_LIB(file)    LoadLibrary(file)
#define PI_PROCESS_ADDR              GetProcAddress
#define PI_UNLOAD_DYNAMIC_LIB(inst)  (FreeLibrary(inst) != 0)
#define PI_EXPORT                    __declspec(dllexport)

#define PI_FUNCTION_MACRO  __FUNCTION__

// String functions
#define STRCPY(dest,size,src)               strcpy_s(dest,size,src)
#define STRNCPY(dest,destsize,src,srcsize)  strncpy_s(dest,destsize,src,srcsize)
#define SPRINTF(dest,size,...)              sprintf_s(dest,size,__VA_ARGS__)
#define SNPRINTF(dest,size,...)             _snprintf_s(dest,size,size,__VA_ARGS__)
#define STD_SNPRINTF(dest,size,...)         _snprintf(dest,size,__VA_ARGS__)
#define SSCANF(dest,format,...)             sscanf_s(dest,format,__VA_ARGS__)
#define STRICMP(dest,src)                   _stricmp(dest,src)
#define PI_THREAD_YIELD                     SwitchToThread
#pragma warning(disable: 4996) // for std::snprintf
#define usleep(usecs)  std::this_thread::sleep_for(std::chrono::microseconds(usecs));
#define pi_GetComputerName(name,cbName)  GetComputerNameA(name,&cbName)
#define PI_MAX_HOSTNAMELEN               MAX_COMPUTERNAME_LENGTH

#elif __linux__
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

typedef unsigned int DWORD;
typedef void* FARPROC;

#define FAR

#if (__cplusplus < 201103L) // If we're using a pre-C++11 compiler
#define PI_FUNCTION_MACRO __FUNCTION__ // gcc's old version
#else
#define PI_FUNCTION_MACRO __func__     // New standardized C++11 version
#endif

// String functions
#define STRCPY(dest,size,src)               strcpy(dest,src)
#define STRNCPY(dest,destsize,src,srcsize)  strncpy(dest,src,srcsize)

#define FOPEN(name,mode)   fopen(name,mode)
#define GETENV(name)       getenv(name)
#define STRICMP(dest,src)  strcasecmp(dest,src)
#define PI_THREAD_YIELD    pthread_yield

// Cross platform loading of plugins
#include <dlfcn.h>
typedef void* PI_INSTANCE;
#define PI_LOAD_DYNAMIC_LIB(file)     dlopen(file,RTLD_LAZY)
#define PI_PROCESS_ADDR               dlsym
#define PI_UNLOAD_DYNAMIC_LIB(inst)   dlclose(inst) == 0
#define PI_EXPORT

// Only C++ has "inline" on Linux. On Windows, _inline works in both C and C++.
#ifdef __cplusplus
#define SPRINTF(dest,size,...)       std::sprintf(dest,__VA_ARGS__)
#define SNPRINTF(dest,size,...)      std::snprintf(dest,size,__VA_ARGS__)
#define STD_SNPRINTF(dest,size,...)  std::snprintf(dest,size,__VA_ARGS__)
#define SSCANF(dest,format,...)      std::sscanf(dest,format,__VA_ARGS__)

// With C++11, cross-platform sleep is built-in.
// Should really just use this directly. But in here for the lazy, backwards compatibility.
#define Sleep(msecs)  std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
#else
#define SPRINTF(dest,size,...)   std::sprintf(dest,__VA_ARGS__)
#define SNPRINTF(dest,size,...)  std::snprintf(dest,size,__VA_ARGS__)
#define SSCANF(dest,format,...)  std::sscanf(dest,format,__VA_ARGS__)
#endif // __cplusplus
#define pi_GetComputerName(name,cbName)  gethostname(name,cbName)
#define PI_MAX_HOSTNAMELEN               MAXHOSTNAMELEN
#endif // WIN32

namespace pi
{
#ifdef WIN32
    // On Windows, _inline works for either C or C++
    _inline FILE* FOPEN(const char* name, const char* mode)
    {
        FILE* f;
        errno_t err = fopen_s(&f, name, mode);
        if (err == 0)
        {
            return f;
        }
        return NULL;
    }

    //
    // "dupenv" mallocs memory for the string. You should free() it,
    // but that kind of destroys the portability.  Not calling free()
    // shouldn't be a problem unless you're calling this in a loop,
    // which is usually not the case with getenv() calls
    //
    _inline char* GETENV(const char* name)
    {
        char* pValue;
        size_t  len;
        _dupenv_s(&pValue, &len, name);
        return pValue;
    }

    // With C++11, cross-platform sleep is built-in.
    // Should really just use this directly. But in here for the lazy, backwards compatibility.
    //#define sleep(secs)    std::this_thread::sleep_for(std::chrono::seconds(secs));

#elif __linux__

//#include <X11/XKBlib.h>
//
// GetTickCount is a Windows function that returns the number of seconds
// since the system started.
//
#ifdef __cplusplus
    inline double GetTickCount(void)
    {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now))
        {
            return 0;
        }
        return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
    }
#endif

    inline bool pi_getCwd(char(&buf)[260])
    {
        memset(buf, '\0', sizeof(buf)); // This shouldn't be needed, but sometimes gave weird runtimes otherwise
        if (readlink("/proc/self/exe", buf, sizeof(buf)) != -1)
        {
            // buf will be set.
            return true;
        }
        else
        {
            // Not sure we are guaranteed buf is still null
            buf[0] = 0;
            return false;
        }
    }

#endif

    /// Returns the full path to the executable currently running.
    inline std::filesystem::path GetExecutablePath()
    {
        const int nPathIncrementSize = 200;
        std::string strExecutablePath;

#if defined( WIN32 ) || defined( WIN64 )
        DWORD nBufferExePathSize = 0;
        DWORD nSizeReturned = 0;
        // GetModuleFileName returns the size copied to the supplied buffer, if it
        // fills the entire buffer then the file path was too long and it truncated
        // it. Resize buffer and try again until the full file path is successfully
        // retrieved.

#if defined(UNICODE)
        std::vector<WCHAR> pathVector;  //WCHAR vector gets converted to a vector of chars
        while (nSizeReturned == nBufferExePathSize) {
            nBufferExePathSize += nPathIncrementSize;
            pathVector.resize(nBufferExePathSize, '\0');
            nSizeReturned = GetModuleFileName(nullptr, pathVector.data(), nBufferExePathSize);
        }
        // Resize one last time now that the final size has been determined
        pathVector.resize(nSizeReturned);
        strExecutablePath.resize(nSizeReturned);

        wcstombs(&strExecutablePath[0], pathVector.data(), pathVector.size());
#else
        while (nSizeReturned == nBufferExePathSize) {
            nBufferExePathSize += nPathIncrementSize;
            strExecutablePath.resize(nBufferExePathSize, '\0');
            nSizeReturned = GetModuleFileName(nullptr, &strExecutablePath[0], nBufferExePathSize);
        }
        // Resize one last time now that the final size has been determined
        strExecutablePath.resize(nSizeReturned);
#endif

#else  //NOT defined( WIN32 ) || defined( WIN64 )
        size_t  nBufferExePathSize = 0;
        ssize_t nSizeReturned = 0;
        // readlink returns the size copied to the supplied buffer, if it fills the
        // entire buffer then the file path was too long and it truncated it. Resize
        // buffer and try again until the full file path is successfully retrieved.
        while (nBufferExePathSize == static_cast<size_t>(nSizeReturned)) {
            nBufferExePathSize += nPathIncrementSize;
            strExecutablePath.resize(nBufferExePathSize, '\0');
            nSizeReturned = readlink("/proc/self/exe", &strExecutablePath[0], nBufferExePathSize);
            if (nSizeReturned < 0) {
                throw std::runtime_error("Error finding the executable path.");
            }
        }
        strExecutablePath.resize(nSizeReturned);
#endif

        return std::filesystem::path(strExecutablePath);
    }

    /// Returns the full path of the directory containing the executable currently running.
    inline std::filesystem::path GetExecutableDirectory()
    {
        std::filesystem::path result = GetExecutablePath();
        result.remove_filename();
        return result;
    }


    inline std::string GetEnvVar(const std::string& var)
    {
        std::string res;

#ifdef WIN32
        size_t varLength;
        getenv_s(&varLength, nullptr, 0, var.c_str());
        if (varLength > 0) {
            res.resize(varLength);
            getenv_s(&varLength,
                &res[0],
                res.size(),
                var.c_str());
            res.resize(varLength - 1);    // remove null
        }
#else
        const char* s = getenv(var.c_str());
        if (s != nullptr) {
            res = s;
        }
#endif

        return res;
    }

    template<typename ...Types>
    std::string PI_STRING_FORMAT(const std::string& format, Types... args)
    {
        size_t bufsize = STD_SNPRINTF(nullptr, 0, format.c_str(), args...) + 1; // extra +1 for null terminator
        std::unique_ptr<char[]> buf(new char[bufsize]);
        STD_SNPRINTF(buf.get(), bufsize, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + bufsize - 1); // Don't want the null terminator
    }

    // Get username of the currently logged-in individual
    inline const std::string GetUsername()
    {
        std::string userName;
#ifdef WIN32
        char buffer[100];
        DWORD size = 100;
        if (GetUserNameA(buffer, &size)) {
            userName = buffer;
        }
#else
        char buffer[100];
        size_t size = 100;
        //Linux command returns zero on success
        if (getlogin_r(buffer, size) == 0) {
            userName = buffer;
        }
#endif
        return userName;
    }
}
