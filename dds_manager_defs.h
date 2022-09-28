#ifndef __DDS_MANAGER_DEFS__
#define __DDS_MANAGER_DEFS__

// Define DLL_PUBLIC as the import or export symbol declaration
#if defined WIN32 || defined WIN64 || defined __CYGWIN__
  #ifdef OpenDDSManager_EXPORTS // Set automatically by CMake
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllexport))
    #else
      #define DLL_PUBLIC __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllimport))
    #else
      #define DLL_PUBLIC __declspec(dllimport)
    #endif
  #endif
#else
  #if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
  #else
    #define DLL_PUBLIC
  #endif
#endif

#endif

/**
 * @}
 */
