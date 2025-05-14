# source:
# https://github.com/NVIDIA/tensorrt-laboratory/blob/master/cmake/FindTensorRT.cmake

# This module defines the following variables:
#
# ::
#
#   TensorRT_INCLUDE_DIRS
#   TensorRT_LIBRARIES
#   TensorRT_FOUND
#
# ::
#
#   TensorRT_VERSION_STRING - version (x.y.z)
#   TensorRT_VERSION_MAJOR  - major version (x)
#   TensorRT_VERSION_MINOR  - minor version (y)
#   TensorRT_VERSION_PATCH  - patch version (z)
#
# Hints
# ^^^^^
# A user may set ``TensorRT_DIR`` to an installation root to tell this module where to look.
#
set(_TensorRT_SEARCHES)

if(TensorRT_DIR)
    set(_TensorRT_SEARCH_ROOT PATHS ${TensorRT_DIR} NO_DEFAULT_PATH)
    list(APPEND _TensorRT_SEARCHES _TensorRT_SEARCH_ROOT)
endif()

# appends some common paths
set(_TensorRT_SEARCH_NORMAL
        PATHS "/usr"
        )
list(APPEND _TensorRT_SEARCHES _TensorRT_SEARCH_NORMAL)

# Include dir
foreach(search ${_TensorRT_SEARCHES})
    find_path(TensorRT_INCLUDE_DIR NAMES NvInfer.h ${${search}} PATH_SUFFIXES include)
endforeach()

if(NOT TensorRT_LIBRARY)
    foreach(search ${_TensorRT_SEARCHES})
        find_library(TensorRT_LIBRARY NAMES nvinfer_10 ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
if(NOT TensorRT_LIBRARY)
    message(FATAL_ERROR "tensorrt not found")
endif()

if(NOT TensorRT_NVONNXPARSER_LIBRARY)
    foreach(search ${_TensorRT_SEARCHES})
        find_library(TensorRT_NVONNXPARSER_LIBRARY NAMES nvonnxparser_10 ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
if(NOT TensorRT_NVONNXPARSER_LIBRARY)
    message(FATAL_ERROR "onnxparser not found")
endif()

if(NOT TensorRT_PLUGIN_LIBRARY)
    foreach(search ${_TensorRT_SEARCHES})
        find_library(TensorRT_PLUGIN_LIBRARY NAMES nvinfer_plugin_10 ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
if(NOT TensorRT_PLUGIN_LIBRARY)
    message(FATAL_ERROR "tensorrt plugin not found")
endif()

if(NOT TensorRT_VCPLUGIN_LIBRARY)
    foreach(search ${_TensorRT_SEARCHES})
        find_library(TensorRT_VCPLUGIN_LIBRARY NAMES nvinfer_vc_plugin_10 ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
if(NOT TensorRT_VCPLUGIN_LIBRARY)
    message(FATAL_ERROR "tensorrt vcplugin not found")
endif()

if(NOT TensorRT_LEAN_LIBRARY)
    foreach(search ${_TensorRT_SEARCHES})
        find_library(TensorRT_LEAN_LIBRARY NAMES nvinfer_lean_10 ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
if(NOT TensorRT_LEAN_LIBRARY)
    message(FATAL_ERROR "tensorrt lean not found")
endif()

if(NOT TensorRT_DISPATCH_LIBRARY)
    foreach(search ${_TensorRT_SEARCHES})
        find_library(TensorRT_DISPATCH_LIBRARY NAMES nvinfer_dispatch_10 ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
if(NOT TensorRT_DISPATCH_LIBRARY)
    message(FATAL_ERROR "tensorrt dispatch not found")
endif()

set(TensorRT_INFER_BUILDER_RESOURCE_DLL ${TensorRT_DIR}/lib/nvinfer_builder_resource_10.dll)

mark_as_advanced(TensorRT_INCLUDE_DIR)

if(TensorRT_INCLUDE_DIR AND EXISTS "${TensorRT_INCLUDE_DIR}/NvInfer.h")
    file(STRINGS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h" TensorRT_MAJOR REGEX "^#define NV_TENSORRT_MAJOR [0-9]+.*$")
    file(STRINGS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h" TensorRT_MINOR REGEX "^#define NV_TENSORRT_MINOR [0-9]+.*$")
    file(STRINGS "${TensorRT_INCLUDE_DIR}/NvInferVersion.h" TensorRT_PATCH REGEX "^#define NV_TENSORRT_PATCH [0-9]+.*$")

    string(REGEX REPLACE "^#define NV_TENSORRT_MAJOR ([0-9]+).*$" "\\1" TensorRT_VERSION_MAJOR "${TensorRT_MAJOR}")
    string(REGEX REPLACE "^#define NV_TENSORRT_MINOR ([0-9]+).*$" "\\1" TensorRT_VERSION_MINOR "${TensorRT_MINOR}")
    string(REGEX REPLACE "^#define NV_TENSORRT_PATCH ([0-9]+).*$" "\\1" TensorRT_VERSION_PATCH "${TensorRT_PATCH}")
    set(TensorRT_VERSION_STRING "${TensorRT_VERSION_MAJOR}.${TensorRT_VERSION_MINOR}.${TensorRT_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TensorRT REQUIRED_VARS TensorRT_LIBRARY TensorRT_INCLUDE_DIR VERSION_VAR TensorRT_VERSION_STRING)

if(TensorRT_FOUND)
    set(TensorRT_INCLUDE_DIRS ${TensorRT_INCLUDE_DIR})

    if(NOT TensorRT_LIBRARIES)
        set(TensorRT_LIBRARIES ${TensorRT_LIBRARY} ${TensorRT_NVONNXPARSER_LIBRARY} ${TensorRT_NVPARSERS_LIBRARY} ${TensorRT_DISPATCH_LIBRARY} ${TensorRT_LEAN_LIBRARY} ${TensorRT_PLUGIN_LIBRARY} ${TensorRT_VCPLUGIN_LIBRARY})
    endif()

    if(NOT TARGET TensorRT::TensorRT)
        add_library(TensorRT::TensorRT UNKNOWN IMPORTED)
        set_target_properties(TensorRT::TensorRT PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${TensorRT_INCLUDE_DIRS}")
        set_property(TARGET TensorRT::TensorRT APPEND PROPERTY IMPORTED_LOCATION "${TensorRT_LIBRARY}")
    endif()
endif()


LIST(TRANSFORM TensorRT_LIBRARIES REPLACE "\\.lib" ".dll" OUTPUT_VARIABLE TensorRT_DLLS)
LIST(APPEND TensorRT_DLLS ${TensorRT_INFER_BUILDER_RESOURCE_DLL})
message("${TensorRT_DLLS}")
