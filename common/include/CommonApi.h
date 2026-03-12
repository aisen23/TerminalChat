#pragma once

#ifdef _WIN32
    #ifdef TC_COMMON_BUILD_DLL
        #define TC_COMMON_API __declspec(dllexport)
    #else
        #define TC_COMMON_API __declspec(dllimport)
    #endif
#else
    #define TC_COMMON_API __attribute__((visibility("default")))
#endif
