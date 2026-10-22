# Copyright 2019-2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required (VERSION 3.31.8)

project(tritonpytorchbackend LANGUAGES C CXX)

# Use C++20 standard as Triton's minimum required.
# A stale CMake cache from a build tree configured before this bump would
# otherwise keep TRITON_MIN_CXX_STANDARD at its old cached value, so raise it
# to the new floor while still allowing a user to cache a higher override.
if(DEFINED TRITON_MIN_CXX_STANDARD AND TRITON_MIN_CXX_STANDARD LESS 20)
  set(TRITON_MIN_CXX_STANDARD 20 CACHE STRING "The minimum C++ standard which features are requested to build this target." FORCE)
else()
  set(TRITON_MIN_CXX_STANDARD 20 CACHE STRING "The minimum C++ standard which features are requested to build this target.")
endif()

#
# Options
#
# To build the PyTorch backend you must either:
#
#   - Point to the already built PyTorch and Torchvision using
#     TRITON_PYTORCH_INCLUDE_PATHS and TRITON_PYTORCH_LIB_PATHS
#
#   or:
#
#   - Set TRITON_PYTORCH_DOCKER_IMAGE to use the docker image of
#     PyTorch to base the build off.
#

option(TRITON_ENABLE_GPU "Enable GPU support in backend" ON)
option(TRITON_ENABLE_STATS "Include statistics collections in backend" ON)
option(TRITON_ENABLE_NVTX "Include nvtx markers collection in backend." OFF)
option(TRITON_PYTORCH_ENABLE_TORCHTRT "Enable TorchTRT support" OFF)
option(TRITON_PYTORCH_ENABLE_TORCHVISION "Enable Torchvision support" ON)
option(TRITON_PYTORCH_NVSHMEM "Enable NVSHMEM support" ON)
option(TRITON_PYTORCH_OPTREE "Enable OpTree support" OFF)
option(TRITON_PYTORCH_ENABLE_PT2 "Enable PyTorch 2 AOTInductor (torch_aoti) support" ON)

# HCU (Hygon DTK / ROCm HIP) support. When ON the backend is built with
# hipcc/HIP against the installed HCU Triton backend and the DTK PyTorch
# distribution.
option(USE_HCU "Build with Hygon HCU (DTK/HIP) instead of NVIDIA CUDA" OFF)

set(TRITON_PYTORCH_DOCKER_IMAGE "" CACHE STRING "Docker image containing the PyTorch build required by backend.")
set(TRITON_PYTORCH_INCLUDE_PATHS "" CACHE PATH "Paths to Torch includes")
set(TRITON_PYTORCH_LIB_PATHS "" CACHE PATH "Paths to Torch libraries")

# The backend is built against an already installed Triton (headers, static
# libraries and CMake package configs), never against source checkouts.
# TRITON_CORE_PATH / TRITON_BACKEND_PATH default to the same-named
# environment variables and are only used to extend CMAKE_PREFIX_PATH; any
# prefix containing lib/cmake/TritonCore also works via CMAKE_PREFIX_PATH.
set(TRITON_CORE_PATH "$ENV{TRITON_CORE_PATH}" CACHE PATH "Path to installed Triton core")
set(TRITON_BACKEND_PATH "$ENV{TRITON_BACKEND_PATH}" CACHE PATH "Path to installed Triton backend")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Python.h needed by torch headers.
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)

# Auto-detect the PyTorch installation from the Python environment unless
# the caller provided explicit paths or a docker image.
if(TRITON_PYTORCH_LIB_PATHS STREQUAL "" AND TRITON_PYTORCH_DOCKER_IMAGE STREQUAL "")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import torch, os; print(os.path.dirname(torch.__file__))"
    OUTPUT_VARIABLE TRITON_PYTORCH_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE TRITON_PYTORCH_FIND_STATUS)
  if(NOT TRITON_PYTORCH_FIND_STATUS EQUAL 0 OR TRITON_PYTORCH_ROOT STREQUAL "")
    message(FATAL_ERROR
      "Unable to locate PyTorch from the Python environment. Install PyTorch "
      "or set TRITON_PYTORCH_INCLUDE_PATHS / TRITON_PYTORCH_LIB_PATHS.")
  endif()
  set(TRITON_PYTORCH_INCLUDE_PATHS
      "${TRITON_PYTORCH_ROOT}/include;${TRITON_PYTORCH_ROOT}/include/torch/csrc/api/include")
  set(TRITON_PYTORCH_LIB_PATHS "${TRITON_PYTORCH_ROOT}/lib")
  message(STATUS "Auto-detected PyTorch at ${TRITON_PYTORCH_ROOT}")
endif()

set(TRITON_PYTORCH_DOCKER_BUILD OFF)
if(TRITON_PYTORCH_LIB_PATHS STREQUAL "")
  if(TRITON_PYTORCH_DOCKER_IMAGE STREQUAL "")
    message(FATAL_ERROR "Using the PyTorch docker based build requires TRITON_PYTORCH_DOCKER_IMAGE")
  endif()
  set(TRITON_PYTORCH_DOCKER_BUILD ON)
  message(STATUS "Using PyTorch docker: ${TRITON_PYTORCH_DOCKER_IMAGE}")
else()
  if(NOT TRITON_PYTORCH_ROOT)
    get_filename_component(TRITON_PYTORCH_ROOT "${TRITON_PYTORCH_LIB_PATHS}" DIRECTORY)
  endif()

  # Torch-TRT is optional and only usable when its runtime library exists.
  if(TRITON_PYTORCH_ENABLE_TORCHTRT AND NOT EXISTS "${TRITON_PYTORCH_LIB_PATHS}/libtorchtrt_runtime.so")
    message(WARNING "TRITON_PYTORCH_ENABLE_TORCHTRT is on, but TRITON_PYTORCH_LIB_PATHS does not contain Torch-TRT package")
  endif()

  # TorchVision is optional: enable it only when the runtime library exists.
  find_library(LIBTORCHVISION libtorchvision.so libtorchvision.so.1 PATHS ${TRITON_PYTORCH_LIB_PATHS})
  if(NOT LIBTORCHVISION AND TRITON_PYTORCH_ENABLE_TORCHVISION)
    message(STATUS "TorchVision not found in ${TRITON_PYTORCH_LIB_PATHS}; disabling TorchVision support")
    set(TRITON_PYTORCH_ENABLE_TORCHVISION OFF CACHE BOOL "Enable Torchvision support" FORCE)
  endif()

  # NVSHMEM is only shipped by NVIDIA PyTorch builds.
  if(TRITON_PYTORCH_NVSHMEM AND NOT EXISTS "${TRITON_PYTORCH_LIB_PATHS}/libtorch_nvshmem.so")
    message(STATUS "NVSHMEM library not found; disabling NVSHMEM support")
    set(TRITON_PYTORCH_NVSHMEM OFF CACHE BOOL "Enable NVSHMEM support" FORCE)
  endif()

  # PyTorch 2 AOTInductor (torch_aoti / PT2) requires the AOTI package
  # loader headers; distributions without them cannot use it.
  if(TRITON_PYTORCH_ENABLE_PT2 AND NOT EXISTS
     "${TRITON_PYTORCH_ROOT}/include/torch/csrc/inductor/aoti_package/model_package_loader.h")
    message(STATUS "AOTInductor headers not found in ${TRITON_PYTORCH_ROOT}; disabling PT2 (torch_aoti) support")
    set(TRITON_PYTORCH_ENABLE_PT2 OFF CACHE BOOL "Enable PyTorch 2 AOTInductor (torch_aoti) support" FORCE)
  endif()
endif()

set(RHEL_BUILD OFF)
set(LIB_DIR "lib")
set(LIBTORCH_LIBS_PATH "/usr/local/lib")
set(PY_INSTALL_PATH "/usr/local/lib/python3.12/dist-packages")
if(LINUX)
  file(STRINGS "/etc/os-release" DISTRO_ID_LIKE REGEX "ID_LIKE")
  if(${DISTRO_ID_LIKE} MATCHES "rhel|centos")
    set(RHEL_BUILD ON)
    set(LIB_DIR "lib64")
    set(PY_INSTALL_PATH "/opt/_internal/cpython-3.12.13/lib/python3.12/site-packages")
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
      set(LIBTORCH_LIBS_PATH "/opt/_internal/cpython-3.12.13/lib")
    endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
  endif(${DISTRO_ID_LIKE} MATCHES "rhel|centos")
endif(LINUX)

message(TRACE "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(TRACE "TRITON_ENABLE_GPU: ${TRITON_ENABLE_GPU}")
message(TRACE "TRITON_ENABLE_STATS: ${TRITON_ENABLE_STATS}")
message(TRACE "TRITON_ENABLE_NVTX: ${TRITON_ENABLE_NVTX}")
message(TRACE "TRITON_PYTORCH_ENABLE_TORCHTRT: ${TRITON_PYTORCH_ENABLE_TORCHTRT}")
message(TRACE "TRITON_PYTORCH_ENABLE_TORCHVISION: ${TRITON_PYTORCH_ENABLE_TORCHVISION}")
message(TRACE "TRITON_PYTORCH_NVSHMEM: ${TRITON_PYTORCH_NVSHMEM}")
message(TRACE "TRITON_PYTORCH_OPTREE: ${TRITON_PYTORCH_OPTREE}")
message(TRACE "TRITON_PYTORCH_DOCKER_IMAGE: ${TRITON_PYTORCH_DOCKER_IMAGE}")
message(TRACE "TRITON_PYTORCH_INCLUDE_PATHS: ${TRITON_PYTORCH_INCLUDE_PATHS}")
message(TRACE "TRITON_PYTORCH_LIB_PATHS: ${TRITON_PYTORCH_LIB_PATHS}")
message(TRACE "TRITON_CORE_PATH: ${TRITON_CORE_PATH}")
message(TRACE "TRITON_BACKEND_PATH: ${TRITON_BACKEND_PATH}")
message(TRACE "TRITON_PYTORCH_ROOT: ${TRITON_PYTORCH_ROOT}")
message(TRACE "TRITON_PYTORCH_ENABLE_PT2: ${TRITON_PYTORCH_ENABLE_PT2}")
message(TRACE "TRITON_PYTORCH_DOCKER_BUILD: ${TRITON_PYTORCH_DOCKER_BUILD}")
message(TRACE "RHEL_BUILD: ${RHEL_BUILD}")
message(TRACE "LIB_DIR: ${LIB_DIR}")
message(TRACE "LIBTORCH_LIBS_PATH: ${LIBTORCH_LIBS_PATH}")
message(TRACE "PY_INSTALL_PATH: ${PY_INSTALL_PATH}")

#
# Dependencies (already installed Triton core / backend)
#
if(TRITON_CORE_PATH)
  list(APPEND CMAKE_PREFIX_PATH "${TRITON_CORE_PATH}")
endif()
if(TRITON_BACKEND_PATH)
  list(APPEND CMAKE_PREFIX_PATH "${TRITON_BACKEND_PATH}")
endif()

# Triton core/backend are consumed from an existing installation: only
# the public headers, the static backend utilities and the core stub
# library are required -- no source checkout is involved.
set(TRITON_INSTALL_PREFIX "")
if(TRITON_CORE_PATH)
  set(TRITON_INSTALL_PREFIX "${TRITON_CORE_PATH}")
elseif(TRITON_BACKEND_PATH)
  set(TRITON_INSTALL_PREFIX "${TRITON_BACKEND_PATH}")
elseif(CMAKE_PREFIX_PATH)
  list(GET CMAKE_PREFIX_PATH 0 TRITON_INSTALL_PREFIX)
endif()
if(NOT TRITON_INSTALL_PREFIX)
  message(FATAL_ERROR
    "Set TRITON_CORE_PATH / TRITON_BACKEND_PATH (or CMAKE_PREFIX_PATH) to "
    "the Triton installation prefix.")
endif()

set(TRITON_INCLUDE_DIR "${TRITON_INSTALL_PREFIX}/include")
set(TRITON_LIB_DIR "${TRITON_INSTALL_PREFIX}/lib")
message(STATUS "Using installed Triton at ${TRITON_INSTALL_PREFIX}")

add_library(triton-core-serverapi INTERFACE IMPORTED)
set_target_properties(triton-core-serverapi PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${TRITON_INCLUDE_DIR}")
add_library(triton-core-backendapi INTERFACE IMPORTED)
set_target_properties(triton-core-backendapi PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${TRITON_INCLUDE_DIR}")
add_library(triton-core-serverstub SHARED IMPORTED)
set_target_properties(triton-core-serverstub PROPERTIES
  IMPORTED_LOCATION "${TRITON_LIB_DIR}/stubs/libtritonserver.so")

add_library(triton-common-async-work-queue STATIC IMPORTED)
set_target_properties(triton-common-async-work-queue PROPERTIES
  IMPORTED_LOCATION "${TRITON_LIB_DIR}/libtritonasyncworkqueue.a")
add_library(triton-common-logging STATIC IMPORTED)
set_target_properties(triton-common-logging PROPERTIES
  IMPORTED_LOCATION "${TRITON_LIB_DIR}/libtritoncommonlogging.a")
add_library(triton-common-error STATIC IMPORTED)
set_target_properties(triton-common-error PROPERTIES
  IMPORTED_LOCATION "${TRITON_LIB_DIR}/libtritoncommonerror.a")
add_library(triton-common-json INTERFACE IMPORTED)
set_target_properties(triton-common-json PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${TRITON_INCLUDE_DIR}")

add_library(kernel_library_new STATIC IMPORTED)
set_target_properties(kernel_library_new PROPERTIES
  IMPORTED_LOCATION "${TRITON_LIB_DIR}/libkernel_library_new.a")

add_library(triton-backend-utils STATIC IMPORTED)
set_target_properties(triton-backend-utils PROPERTIES
  IMPORTED_LOCATION "${TRITON_LIB_DIR}/libtritonbackendutils.a"
  INTERFACE_INCLUDE_DIRECTORIES "${TRITON_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES
    "triton-core-backendapi;triton-core-serverapi;triton-common-async-work-queue;triton-common-json")


# Fetch and Build OpTree if enabled.
if (${TRITON_PYTORCH_OPTREE})
  set(optree_LIB_NAME_SUFFIX ".liboptree.cpython${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}-${CMAKE_LIBRARY_ARCHITECTURE}.so")

  find_package(pybind11 REQUIRED)

  include(ExternalProject)
  # Build OpTree as an external project
  ExternalProject_Add(
    extern-optree
    GIT_REPOSITORY "https://github.com/metaopt/optree.git"
    GIT_TAG "v0.19.0"
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DPython_EXECUTABLE=${Python3_EXECUTABLE}
      -DPython_EXTRA_LIBRARIES=${Python3_LIBRARIES}
      -DPython_INCLUDE_DIRS=${Python3_INCLUDE_DIRS}
      -Dpybind11_DIR=${pybind11_DIR}
      -DPYTHON_MODULE_EXTENSION=${optree_LIB_NAME_SUFFIX}
    INSTALL_COMMAND cp <BINARY_DIR>/lib/_C${optree_LIB_NAME_SUFFIX} ${CMAKE_CURRENT_BINARY_DIR}/_C${optree_LIB_NAME_SUFFIX}
  )

  # Get paths from ExternalProject
  ExternalProject_Get_Property(extern-optree SOURCE_DIR)
  ExternalProject_Get_Property(extern-optree BINARY_DIR)

  set(optree_SOURCE_DIR ${SOURCE_DIR})
  set(optree_BINARY_DIR ${BINARY_DIR})

  message(TRACE "optree_SOURCE_DIR: ${optree_SOURCE_DIR}")
  message(TRACE "optree_BINARY_DIR: ${optree_BINARY_DIR}")
  message(TRACE "pybind11_INCLUDE_DIR: ${pybind11_INCLUDE_DIR}")

  # Create imported library target
  add_library(optree SHARED IMPORTED)
  set_target_properties(optree PROPERTIES
      IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/_C${optree_LIB_NAME_SUFFIX}"
  )
  add_dependencies(optree extern-optree)
endif() # TRITON_PYTORCH_OPTREE

#
# CUDA
#
if(${TRITON_ENABLE_GPU})
  if(USE_HCU)
    # HCU: build with hipcc/HIP. Torch's public headers are CUDA-flavoured
    # (c10/cuda, ATen/cuda), so src/hcu_cuda_shim provides HIP-based
    # <cuda*.h> headers that map the CUDA spellings onto HIP.
    find_package(hip REQUIRED)
    add_compile_definitions(USE_HCU=1)
    if(NOT EXISTS "${TRITON_PYTORCH_ROOT}/include/c10/cuda/impl/cuda_cmake_macros.h")
      # CUDAMacros.h has an explicit opt-out for PyTorch builds that do not
      # generate this header (as in the Hygon DTK distribution).
      add_compile_definitions(C10_CUDA_NO_CMAKE_CONFIGURE_FILE)
    endif()
    message(STATUS "Using HIP/DTK at ${hip_DIR}")
  else()
    find_package(CUDAToolkit REQUIRED)
  endif()
else()
  if (${TRITON_PYTORCH_ENABLE_TORCHTRT})
    message(FATAL_ERROR "TRITON_PYTORCH_ENABLE_TORCHTRT is ON when TRITON_ENABLE_GPU is OFF")
  endif()
endif() # TRITON_ENABLE_GPU

if(TRITON_PYTORCH_ENABLE_PT2)
  add_compile_definitions(TRITON_PYTORCH_ENABLE_PT2=1)
endif()

if(${TRITON_ENABLE_NVTX})
  add_definitions(-DTRITON_ENABLE_NVTX=1)
endif() # TRITON_ENABLE_NVTX

#
# Shared library implementing the Triton Backend API
#
configure_file(src/libtriton_pytorch.ldscript libtriton_pytorch.ldscript COPYONLY)

set(PT_LIBS
    "libc10.so"
    "libc10_cuda.so"
    "libc10_hip.so"
    "libtorch.so"
    "libtorch_cpu.so"
    "libtorch_cuda.so"
    "libtorch_hip.so"
    "libtorch_cuda_linalg.so"
    "libtorch_global_deps.so"
    "libjpeg.so.62"
    "libshm.so"
    "libbackend_with_compiler.so"
    "libaoti_custom_ops.so"
    "libtorch_python.so"
    "libcaffe2_nvrtc.so"
)

if (${TRITON_PYTORCH_NVSHMEM})
  set(PT_LIBS
      ${PT_LIBS}
      "libtorch_nvshmem.so"
  )
endif() # TRITON_PYTORCH_NVSHMEM

if (${TRITON_PYTORCH_ENABLE_TORCHVISION})
  set(PT_LIBS
      ${PT_LIBS}
      libtorchvision.so.1
  )
endif() # TRITON_PYTORCH_ENABLE_TORCHVISION

message(VERBOSE "PT_LIBS: ${PT_LIBS}")

if (${TRITON_PYTORCH_ENABLE_TORCHTRT})
  set(PT_LIBS
      ${PT_LIBS}
      "libtorchtrt.so"
      "libtorchtrt_runtime.so"
  )
endif() # TRITON_PYTORCH_ENABLE_TORCHTRT

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
  set(LIBS_ARCH "aarch64")
  set(LIBTORCH_LIBS
      "libnvpl_blas_core.so.0"
      "libnvpl_blas_ilp64_gomp.so.0"
      "libnvpl_blas_ilp64_seq.so.0"
      "libnvpl_blas_lp64_gomp.so.0"
      "libnvpl_blas_lp64_seq.so.0"
      "libnvpl_lapack_core.so.0"
      "libnvpl_lapack_ilp64_gomp.so.0"
      "libnvpl_lapack_ilp64_seq.so.0"
      "libnvpl_lapack_lp64_gomp.so.0"
      "libnvpl_lapack_lp64_seq.so.0"
  )
else()
  set(LIBS_ARCH "x86_64")
  set(LIBTORCH_LIBS
    "libmkl_avx2.so.1"
    "libmkl_avx512.so.1"
    "libmkl_core.so.1"
    "libmkl_def.so.1"
    "libmkl_gnu_thread.so.1"
    "libmkl_intel_lp64.so.1"
    "libmkl_intel_thread.so.1"
    "libmkl_rt.so.1"
    "libmkl_sequential.so.1"
    "libmkl_vml_def.so.1"
  )
endif()
set(TORCHVISION_LIBS
    libjpeg.so.62
    libpng16.so.16
)

message(TRACE "LIBS_ARCH: ${LIBS_ARCH}")
message(TRACE "LIBTORCH_LIBS: ${LIBTORCH_LIBS}")

# The patchelf commands ensure the MKL libraries are loaded correctly during runtime
# Without these, the framework/backend complains of missing libraries / symbols and
# in some cases leads to segmentation faults.
if (${TRITON_PYTORCH_DOCKER_BUILD})
  string(REPLACE ";" " " LIBTORCH_LIBS_STR "${LIBTORCH_LIBS}")
  string(RANDOM 8 "abcdefghijklmnopqrstuvwxyz" random_id)

  add_custom_command(
    OUTPUT
      ${PT_LIBS}
      ${LIBTORCH_LIBS}
      ${TORCHVISION_LIBS}
      LICENSE.pytorch
      include/torch
      include/torchvision
    COMMAND ${CMAKE_COMMAND} -E make_directory "include/torchvision"
    COMMAND docker pull ${TRITON_PYTORCH_DOCKER_IMAGE}
    COMMAND docker rm pytorch_backend_ptlib || echo "error ignored..." || true
    COMMAND docker create --name pytorch_backend_ptlib ${TRITON_PYTORCH_DOCKER_IMAGE}
    COMMAND /bin/sh -c "for i in ${LIBTORCH_LIBS_STR} ; do echo copying $i && docker cp -L pytorch_backend_ptlib:${LIBTORCH_LIBS_PATH}/$i $i ; done"
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libc10.so libc10.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libc10_cuda.so libc10_cuda.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch.so libtorch.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch_cpu.so libtorch_cpu.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch_cuda.so libtorch_cuda.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch_cuda_linalg.so libtorch_cuda_linalg.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch_global_deps.so libtorch_global_deps.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libcaffe2_nvrtc.so libcaffe2_nvrtc.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libshm.so libshm.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libbackend_with_compiler.so libbackend_with_compiler.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libaoti_custom_ops.so libaoti_custom_ops.so
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch_python.so libtorch_python.so
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_NVSHMEM} = 'ON' ]; then docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/lib/libtorch_nvshmem.so libtorch_nvshmem.so; fi"
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_ENABLE_TORCHVISION} = 'ON' ]; then docker cp -a -L pytorch_backend_ptlib:/usr/local/${LIB_DIR}/libtorchvision.so.1 libtorchvision.so.1; fi;"
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_ENABLE_TORCHVISION} = 'ON' ]; then docker cp pytorch_backend_ptlib:/opt/pytorch/vision/torchvision/csrc include/torchvision/torchvision; fi"
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_ENABLE_TORCHTRT} = 'ON' ]; then docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch_tensorrt/lib/libtorchtrt.so libtorchtrt.so; fi"
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_ENABLE_TORCHTRT} = 'ON' ]; then docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch_tensorrt/lib/libtorchtrt_runtime.so libtorchtrt_runtime.so; fi"
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_ENABLE_TORCHTRT} = 'ON' ]; then docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch_tensorrt/bin/torchtrtc torchtrtc; fi"
    COMMAND docker cp pytorch_backend_ptlib:/opt/pytorch/pytorch/LICENSE LICENSE.pytorch
    COMMAND docker cp pytorch_backend_ptlib:${PY_INSTALL_PATH}/torch/include include/torch
    COMMAND docker cp pytorch_backend_ptlib:/opt/pytorch/pytorch/torch/csrc/jit/codegen include/torch/torch/csrc/jit/.

    COMMAND /bin/sh -c "if [ ${RHEL_BUILD} = 'ON' ]; then docker cp -L pytorch_backend_ptlib:/usr/lib64/libjpeg.so.62 libjpeg.so.62; else docker cp -L pytorch_backend_ptlib:/usr/local/lib/libjpeg.so.62 libjpeg.so.62 ; fi;"
    COMMAND /bin/sh -c "if [ ${RHEL_BUILD} = 'ON' ]; then docker cp -L pytorch_backend_ptlib:/usr/lib64/libpng16.so.16 libpng16.so.16; else docker cp -L pytorch_backend_ptlib:/usr/lib/${LIBS_ARCH}-linux-gnu/libpng16.so.16 libpng16.so.16; fi;"
    COMMAND /bin/sh -c "if [ -f libmkl_def.so.1 ]; then patchelf --add-needed libmkl_gnu_thread.so.1 libmkl_def.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_def.so.1 ]; then patchelf --add-needed libmkl_core.so.1 libmkl_def.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_avx2.so.1 ]; then patchelf --add-needed libmkl_gnu_thread.so.1 libmkl_avx2.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_avx2.so.1 ]; then patchelf --add-needed libmkl_core.so.1 libmkl_avx2.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_avx512.so.1 ]; then patchelf --add-needed libmkl_gnu_thread.so.1 libmkl_avx512.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_avx512.so.1 ]; then patchelf --add-needed libmkl_core.so.1 libmkl_avx512.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_vml_def.so.1 ]; then patchelf --add-needed libmkl_gnu_thread.so.1 libmkl_vml_def.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_vml_def.so.1 ]; then patchelf --add-needed libmkl_intel_thread.so.1 libmkl_vml_def.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_vml_def.so.1 ]; then patchelf --add-needed libmkl_core.so.1 libmkl_vml_def.so.1; fi"
    COMMAND /bin/sh -c "if [ -f libmkl_intel_thread.so.1 ]; then patchelf --add-needed libmkl_intel_lp64.so.1 libmkl_intel_thread.so.1; fi"
    COMMAND /bin/sh -c "if [ ${TRITON_PYTORCH_ENABLE_TORCHVISION} = 'ON' ]; then ln -s libtorchvision.so.1 libtorchvision.so; fi;"
    COMMAND docker rm pytorch_backend_ptlib
    COMMENT "Extracting pytorch and torchvision libraries and includes from ${TRITON_PYTORCH_DOCKER_IMAGE}"
    VERBATIM
  )
  add_custom_target(ptlib_target DEPENDS ${PT_LIBS} ${LIBTORCH_LIBS} ${TORCHVISION_LIBS})
  add_library(ptlib SHARED IMPORTED GLOBAL)
  add_dependencies(ptlib ptlib_target)

  # Just one of the libs are enough to ensure the docker build
  set_target_properties(
    ptlib
    PROPERTIES
      IMPORTED_LOCATION libtorch.so
  )
endif() # TRITON_PYTORCH_DOCKER_BUILD

add_library(
  triton-pytorch-backend SHARED
  src/libtorch.cc
  src/libtorch_utils.cc
  src/libtorch_utils.h
  src/string_utils.cc
  src/triton_utils.cc
  src/triton_exception.cc
  src/pt/model_state.cc
  src/pt/model_instance_state.cc
)

# PT2 (torch_aoti) sources are only built when the installed PyTorch
# distribution provides the AOTInductor package loader.
if(TRITON_PYTORCH_ENABLE_PT2)
  target_sources(
    triton-pytorch-backend PRIVATE
    src/pt2/model_state.cc
    src/pt2/model_instance_state.cc
    src/pt2/call_spec.cc
    src/pt2/io_data.cc
  )
endif()

add_library(
  TritonPyTorchBackend::triton-pytorch-backend ALIAS triton-pytorch-backend
)

target_include_directories(
  triton-pytorch-backend
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Python3_INCLUDE_DIRS}
    $<$<BOOL:${TRITON_PYTORCH_OPTREE}>:${optree_SOURCE_DIR}/include>
)

if (${TRITON_PYTORCH_DOCKER_BUILD})
  target_include_directories(
    triton-pytorch-backend
    PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/include/torch
      ${CMAKE_CURRENT_BINARY_DIR}/include/torch/torch/csrc/api/include
      ${CMAKE_CURRENT_BINARY_DIR}/include/torchvision
  )
else()
  target_include_directories(
    triton-pytorch-backend
    PRIVATE
      ${TRITON_PYTORCH_INCLUDE_PATHS}
  )
endif() # TRITON_PYTORCH_DOCKER_BUILD

if(USE_HCU)
  # HIP-based <cuda*.h> shims for Torch's CUDA-flavoured public headers.
  target_include_directories(
    triton-pytorch-backend BEFORE PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src/hcu_cuda_shim
  )
  # Make the CUDA -> HIP runtime mappings visible in every translation unit
  # (the backend and Torch headers use the cuda* spellings).
  target_compile_options(triton-pytorch-backend PRIVATE
    -include "${CMAKE_CURRENT_SOURCE_DIR}/src/hcu_cuda_shim/cuda_runtime_api.h")
endif()

# Need to turn off -Werror due to Torchvision vision.h extern initialization
# Unfortunately gcc does not provide a specific flag to ignore the specific
# warning: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=45977
target_compile_features(triton-pytorch-backend PRIVATE cxx_std_${TRITON_MIN_CXX_STANDARD})
target_compile_options(
  triton-pytorch-backend PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wno-unused-parameter -Wno-type-limits>
)

if(${TRITON_ENABLE_GPU})
  target_compile_definitions(
    triton-pytorch-backend
    PRIVATE TRITON_ENABLE_GPU=1
  )
endif() # TRITON_ENABLE_GPU

set_target_properties(
  triton-pytorch-backend
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME triton_pytorch
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
    LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libtriton_pytorch.ldscript
    LINK_FLAGS "-Wl,--no-as-needed,--version-script libtriton_pytorch.ldscript"
)

# Need to turn off unused-but-set-variable due to Torchvision
# Need to turn off unknown-pragmas due to ATen OpenMP
set_target_properties(
  triton-pytorch-backend
  PROPERTIES COMPILE_FLAGS
    "-Wno-unknown-pragmas -Wno-unused-but-set-variable"
)

if (${TRITON_PYTORCH_DOCKER_BUILD})
  add_dependencies(
    triton-pytorch-backend
    ptlib
  )
endif() # TRITON_PYTORCH_DOCKER_BUILD

message(STATUS "Torchvision support is ${TRITON_PYTORCH_ENABLE_TORCHVISION}")
message(STATUS "Torch-TRT support is ${TRITON_PYTORCH_ENABLE_TORCHTRT}")

set(TRITON_PYTORCH_LDFLAGS "")
if (${TRITON_PYTORCH_DOCKER_BUILD})
  set(TRITON_PYTORCH_LIBS "${CMAKE_CURRENT_BINARY_DIR}/libtorch.so")

  if (${TRITON_PYTORCH_ENABLE_TORCHVISION})
    set(TRITON_PYTORCH_LIBS
    ${TRITON_PYTORCH_LIBS}
    libtorchvision.so.1 )
  endif() # TRITON_PYTORCH_ENABLE_TORCHVISION

  if (${TRITON_PYTORCH_ENABLE_TORCHTRT})
    set(TRITON_PYTORCH_LIBS
        ${TRITON_PYTORCH_LIBS}
        "${CMAKE_CURRENT_BINARY_DIR}/libtorchtrt_runtime.so")
  endif() # TRITON_PYTORCH_ENABLE_TORCHTRT
else()
  set (TRITON_PYTORCH_LIBS "-ltorch")

  if (${TRITON_PYTORCH_ENABLE_TORCHVISION})
    set(TRITON_PYTORCH_LIBS
        ${TRITON_PYTORCH_LIBS}
        "-ltorchvision"
    )
  endif() # TRITON_PYTORCH_ENABLE_TORCHVISION

  if (${TRITON_PYTORCH_ENABLE_TORCHTRT})
    set(TRITON_PYTORCH_LIBS
        ${TRITON_PYTORCH_LIBS}
        "-ltorchtrt_runtime"
    )
  endif() # TRITON_PYTORCH_ENABLE_TORCHTRT

  FOREACH(p ${TRITON_PYTORCH_LIB_PATHS})
    set(TRITON_PYTORCH_LDFLAGS ${TRITON_PYTORCH_LDFLAGS} "-L${p}")
  ENDFOREACH(p)
endif() # TRITON_PYTORCH_DOCKER_BUILD

message(TRACE "TRITON_PYTORCH_LDFLAGS: ${TRITON_PYTORCH_LDFLAGS}")
message(TRACE "TRITON_PYTORCH_LIBS: ${TRITON_PYTORCH_LIBS}")

target_link_libraries(
  triton-pytorch-backend
  PRIVATE
    triton-core-serverapi
    triton-core-backendapi
    triton-core-serverstub
    triton-backend-utils
    kernel_library_new
    triton-common-async-work-queue
    triton-common-logging
    triton-common-error
    ${CMAKE_DL_LIBS}       # dlopen/dlsym/dlclose for the MODEL_INIT_LIBRARY hook
    ${TRITON_PYTORCH_LDFLAGS}
    ${TRITON_PYTORCH_LIBS}
    $<$<BOOL:${TRITON_PYTORCH_OPTREE}>:optree>
    $<$<AND:$<BOOL:${TRITON_ENABLE_GPU}>,$<NOT:$<BOOL:${USE_HCU}>>>:CUDA::cudart>
    $<$<AND:$<BOOL:${TRITON_ENABLE_GPU}>,$<BOOL:${USE_HCU}>>:hip::host>
)

#
# Install
#
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/TritonPyTorchBackend)
message(TRACE "INSTALL_CONFIGDIR: ${INSTALL_CONFIGDIR}")

install(
  TARGETS
    triton-pytorch-backend
  EXPORT
    triton-pytorch-backend-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/backends/pytorch
  ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/backends/pytorch
)

if (${TRITON_PYTORCH_DOCKER_BUILD})
  set(PT_LIB_PATHS "")
  FOREACH(plib ${PT_LIBS} ${LIBTORCH_LIBS} ${TORCHVISION_LIBS})
    set(PT_LIB_PATHS ${PT_LIB_PATHS} "${CMAKE_CURRENT_BINARY_DIR}/${plib}")
  ENDFOREACH(plib)

  message(TRACE "PT_LIB_PATHS: ${PT_LIB_PATHS}")

  install(
    FILES
      ${PT_LIB_PATHS}
      $<$<BOOL:${TRITON_PYTORCH_OPTREE}>:${CMAKE_CURRENT_BINARY_DIR}/_C${optree_LIB_NAME_SUFFIX}>
      ${CMAKE_CURRENT_BINARY_DIR}/LICENSE.pytorch
    DESTINATION ${CMAKE_INSTALL_PREFIX}/backends/pytorch
  )

  if (${TRITON_PYTORCH_ENABLE_TORCHTRT})
    install(
      FILES
        ${CMAKE_CURRENT_BINARY_DIR}/torchtrtc
      DESTINATION ${CMAKE_INSTALL_PREFIX}/backends/pytorch
    )
  endif() # TRITON_PYTORCH_ENABLE_TORCHTRT

  FOREACH(plib ${PT_LIBS} ${LIBTORCH_LIBS} ${TORCHVISION_LIBS})
    install(
      CODE
        "EXECUTE_PROCESS(
          COMMAND patchelf --set-rpath \$ORIGIN ${plib}
          RESULT_VARIABLE PATCHELF_STATUS
          WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/backends/pytorch)
        if(PATCHELF_STATUS AND NOT PATCHELF_STATUS EQUAL 0)
          message(FATAL_ERROR \"FAILED: to run patchelf\")
        endif()"
    )
  ENDFOREACH(plib)

else()
  # Keep only the libraries that this PyTorch distribution actually ships
  # (CUDA- vs HIP-named, optional components, ...).
  set(_PT_LIBS_AVAILABLE "")
  foreach(plib ${PT_LIBS})
    if(EXISTS "${TRITON_PYTORCH_LIB_PATHS}/${plib}")
      list(APPEND _PT_LIBS_AVAILABLE ${plib})
    endif()
  endforeach()
  set(PT_LIBS ${_PT_LIBS_AVAILABLE})
  list(REMOVE_DUPLICATES PT_LIBS)

  FOREACH(plib ${PT_LIBS})
    set(PT_LIB_PATHS ${PT_LIB_PATHS} "${TRITON_PYTORCH_LIB_PATHS}/${plib}")
  ENDFOREACH(plib)

  message(TRACE "PT_LIB_PATHS: ${PT_LIB_PATHS}")

  install(
    FILES
      ${PT_LIB_PATHS}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/backends/pytorch
  )

  FOREACH(plib ${PT_LIBS})
    install(
      CODE
        "EXECUTE_PROCESS(
          COMMAND patchelf --set-rpath \$ORIGIN ${plib}
          RESULT_VARIABLE PATCHELF_STATUS
          WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/backends/pytorch)
        if(PATCHELF_STATUS AND NOT PATCHELF_STATUS EQUAL 0)
          message(FATAL_ERROR \"FAILED: to run patchelf\")
        endif()"
    )
  ENDFOREACH(plib)
endif() # TRITON_PYTORCH_DOCKER_BUILD

install(
  EXPORT
    triton-pytorch-backend-targets
  FILE
    TritonPyTorchBackendTargets.cmake
  NAMESPACE
    TritonPyTorchBackend::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

install(
  FILES
    src/model.py
  DESTINATION
    ${CMAKE_INSTALL_PREFIX}/backends/pytorch
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/TritonPyTorchBackendConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TritonPyTorchBackendConfig.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/TritonPyTorchBackendConfig.cmake
  DESTINATION ${INSTALL_CONFIGDIR}
)

#
# Export from build tree
#
export(
  EXPORT triton-pytorch-backend-targets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/TritonPyTorchBackendTargets.cmake
  NAMESPACE TritonPyTorchBackend::
)

export(PACKAGE TritonPyTorchBackend)
