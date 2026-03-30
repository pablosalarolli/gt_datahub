#pragma once

/* ==================== Platform Detection ==================== */
#if defined(_WIN32) || defined(__CYGWIN__)
  #define GT_DATAHUB_WINDOWS 1
#else
  #define GT_DATAHUB_WINDOWS 0
#endif

/* ==================== Shared Library Support ==================== */
#if GT_DATAHUB_WINDOWS
  /* Windows (MSVC/MinGW) */
  #ifdef __GNUC__
    #define GT_DATAHUB_DLL_EXPORT __attribute__((dllexport))
    #define GT_DATAHUB_DLL_IMPORT __attribute__((dllimport))
  #else
    #define GT_DATAHUB_DLL_EXPORT __declspec(dllexport)
    #define GT_DATAHUB_DLL_IMPORT __declspec(dllimport)
  #endif
#else
  /* Linux/macOS */
  #define GT_DATAHUB_DLL_EXPORT __attribute__((visibility("default")))
  #define GT_DATAHUB_DLL_IMPORT
#endif

/* ==================== Public Symbol Definition ==================== */
#if defined(GT_DATAHUB_BUILDING_LIBRARY_SHARED)
  /* Building the shared library */
  #define GT_DATAHUB_PUBLIC GT_DATAHUB_DLL_EXPORT
#elif defined(GT_DATAHUB_USING_LIBRARY_SHARED)
  /* Using the shared library */
  #define GT_DATAHUB_PUBLIC GT_DATAHUB_DLL_IMPORT
#else
  /* Static library or non-library code */
  #define GT_DATAHUB_PUBLIC
#endif

/* ==================== Type Visibility ==================== */
#if GT_DATAHUB_WINDOWS
  #define GT_DATAHUB_PUBLIC_TYPE GT_DATAHUB_PUBLIC
  #define GT_DATAHUB_LOCAL
#else
  /* Non-Windows: Use visibility attributes if available */
  #if __GNUC__ >= 4
    #define GT_DATAHUB_LOCAL __attribute__((visibility("hidden")))
  #else
    #define GT_DATAHUB_LOCAL
  #endif
  /* Public types don't need special attributes on non-Windows */
  #define GT_DATAHUB_PUBLIC_TYPE
#endif
