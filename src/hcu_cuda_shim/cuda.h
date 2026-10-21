// Copyright (c) 2026 Hygon Information Technology Co., Ltd.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
// HCU: HIP-based stand-in for the CUDA driver API header.
#include <hip/hip_runtime_api.h>
#include "cuda_to_hip.h"

// Driver API type spellings.
using CUresult = hipError_t;
using CUdevice = hipDevice_t;
using CUcontext = hipCtx_t;
using CUmodule = hipModule_t;
using CUfunction = hipFunction_t;
using CUstream = hipStream_t;
using CUevent = hipEvent_t;
using CUdeviceptr = hipDeviceptr_t;

#ifndef cuGetErrorString
#define cuGetErrorString hipGetErrorString
#endif
#ifndef cuGetErrorName
#define cuGetErrorName hipGetErrorName
#endif
#ifndef cuDeviceGetAttribute
#define cuDeviceGetAttribute hipDeviceGetAttribute
#endif
#ifndef cuDeviceGet
#define cuDeviceGet hipDeviceGet
#endif
#ifndef cuDeviceGetCount
#define cuDeviceGetCount hipGetDeviceCount
#endif
#ifndef cuDeviceGetName
#define cuDeviceGetName hipDeviceGetName
#endif
#ifndef cuCtxGetCurrent
#define cuCtxGetCurrent hipCtxGetCurrent
#endif
#ifndef cuCtxSetCurrent
#define cuCtxSetCurrent hipCtxSetCurrent
#endif
#ifndef cuModuleLoad
#define cuModuleLoad hipModuleLoad
#endif
#ifndef cuModuleUnload
#define cuModuleUnload hipModuleUnload
#endif
#ifndef cuModuleGetFunction
#define cuModuleGetFunction hipModuleGetFunction
#endif
#ifndef cuLaunchKernel
#define cuLaunchKernel hipModuleLaunchKernel
#endif
