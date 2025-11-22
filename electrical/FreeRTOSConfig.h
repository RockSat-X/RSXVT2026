// @/`FreeRTOS File Inclusion Behavior`:
//
// The actual FreeRTOS configuration is defined within "system.h".
//
// We never directly include this "FreeRTOSConfig.h" file, but
// the FreeRTOS kernel source code will when we compile their
// source files.
//
// However, things are a little wonky because the "FreeRTOS.h"
// header also includes this file, so to prevent some weird
// file inclusion recursion shenanigans, we detect whether or
// not this "FreeRTOSConfig.h" has been included.
//
// If this file was included, then we are part of the
// compilation unit for a FreeRTOS source file, to which we
// should include the FreeRTOS configurations in "system.h".
//
// If this wasn't included, then in "system.h" when "FreeRTOS.h"
// is included (which then includes "FreeRTOSConfigs.h"), we
// avoid including "system.h" to prevent recursion/redefinitions.

#ifndef COMPILING_FREERTOS_SOURCE_FILE
    #define COMPILING_FREERTOS_SOURCE_FILE true
#endif

#if COMPILING_FREERTOS_SOURCE_FILE
    #include "system.h"
#endif
