// HCU (Hygon DTK / ROCm HIP) compatibility mapping.
//
// Torch's public headers are CUDA-flavoured (c10/cuda/*, ATen/cuda/*) and
// include <cuda_runtime_api.h> etc. On the HCU path we compile with hipcc and
// must not mix the CUDA headers with HIP, so the cuda* header names are
// redirected to HIP and the CUDA spellings are mapped onto their HIP
// equivalents here.
#pragma once

#include <hip/hip_runtime.h>

// ---- types ----------------------------------------------------------------
using cudaStream_t = hipStream_t;
using cudaEvent_t = hipEvent_t;
using cudaError_t = hipError_t;
using cudaDeviceProp = hipDeviceProp_t;
using cudaMemcpyKind = hipMemcpyKind;
using cudaStreamCaptureMode = hipStreamCaptureMode;
using cudaStreamCaptureStatus = hipStreamCaptureStatus;
using cudaIpcEventHandle_t = hipIpcEventHandle_t;

// ---- enumerators ----------------------------------------------------------
#ifndef cudaSuccess
#define cudaSuccess hipSuccess
#endif
#ifndef cudaErrorNotReady
#define cudaErrorNotReady hipErrorNotReady
#endif
#ifndef cudaEventDefault
#define cudaEventDefault hipEventDefault
#endif
#ifndef cudaEventDisableTiming
#define cudaEventDisableTiming hipEventDisableTiming
#endif
#ifndef cudaStreamCaptureStatusNone
#define cudaStreamCaptureStatusNone hipStreamCaptureStatusNone
#endif
#ifndef cudaStreamCaptureStatusActive
#define cudaStreamCaptureStatusActive hipStreamCaptureStatusActive
#endif
#ifndef cudaStreamCaptureStatusInvalidated
#define cudaStreamCaptureStatusInvalidated hipStreamCaptureStatusInvalidated
#endif
#ifndef cudaMemcpyHostToDevice
#define cudaMemcpyHostToDevice hipMemcpyHostToDevice
#endif
#ifndef cudaMemcpyDeviceToHost
#define cudaMemcpyDeviceToHost hipMemcpyDeviceToHost
#endif
#ifndef cudaMemcpyDeviceToDevice
#define cudaMemcpyDeviceToDevice hipMemcpyDeviceToDevice
#endif
#ifndef cudaMemcpyDefault
#define cudaMemcpyDefault hipMemcpyDefault
#endif
#ifndef cudaStreamDefault
#define cudaStreamDefault hipStreamDefault
#endif
#ifndef cudaHostAllocPortable
#define cudaHostAllocPortable hipHostMallocPortable
#endif

// ---- runtime API ----------------------------------------------------------
#ifndef cudaGetErrorString
#define cudaGetErrorString hipGetErrorString
#endif
#ifndef cudaGetLastError
#define cudaGetLastError hipGetLastError
#endif
#ifndef cudaGetDevice
#define cudaGetDevice hipGetDevice
#endif
#ifndef cudaSetDevice
#define cudaSetDevice hipSetDevice
#endif
#ifndef cudaGetDeviceCount
#define cudaGetDeviceCount hipGetDeviceCount
#endif
#ifndef cudaGetDeviceProperties
#define cudaGetDeviceProperties hipGetDeviceProperties
#endif
#ifndef cudaDeviceGetStreamPriorityRange
#define cudaDeviceGetStreamPriorityRange hipDeviceGetStreamPriorityRange
#endif
#ifndef cudaDeviceSynchronize
#define cudaDeviceSynchronize hipDeviceSynchronize
#endif
#ifndef cudaStreamCreate
#define cudaStreamCreate hipStreamCreate
#endif
#ifndef cudaStreamCreateWithFlags
#define cudaStreamCreateWithFlags hipStreamCreateWithFlags
#endif
#ifndef cudaStreamCreateWithPriority
#define cudaStreamCreateWithPriority hipStreamCreateWithPriority
#endif
#ifndef cudaStreamDestroy
#define cudaStreamDestroy hipStreamDestroy
#endif
#ifndef cudaStreamSynchronize
#define cudaStreamSynchronize hipStreamSynchronize
#endif
#ifndef cudaStreamQuery
#define cudaStreamQuery hipStreamQuery
#endif
#ifndef cudaStreamWaitEvent
#define cudaStreamWaitEvent hipStreamWaitEvent
#endif
#ifndef cudaStreamGetPriority
#define cudaStreamGetPriority hipStreamGetPriority
#endif
#ifndef cudaStreamIsCapturing
#define cudaStreamIsCapturing hipStreamIsCapturing
#endif
#ifndef cudaStreamBeginCapture
#define cudaStreamBeginCapture hipStreamBeginCapture
#endif
#ifndef cudaStreamEndCapture
#define cudaStreamEndCapture hipStreamEndCapture
#endif
#ifndef cudaThreadExchangeStreamCaptureMode
#define cudaThreadExchangeStreamCaptureMode hipThreadExchangeStreamCaptureMode
#endif
#ifndef cudaEventCreate
#define cudaEventCreate hipEventCreate
#endif
#ifndef cudaEventCreateWithFlags
#define cudaEventCreateWithFlags hipEventCreateWithFlags
#endif
#ifndef cudaEventDestroy
#define cudaEventDestroy hipEventDestroy
#endif
#ifndef cudaEventRecord
#define cudaEventRecord hipEventRecord
#endif
#ifndef cudaEventQuery
#define cudaEventQuery hipEventQuery
#endif
#ifndef cudaEventSynchronize
#define cudaEventSynchronize hipEventSynchronize
#endif
#ifndef cudaEventElapsedTime
#define cudaEventElapsedTime hipEventElapsedTime
#endif
#ifndef cudaIpcGetEventHandle
#define cudaIpcGetEventHandle hipIpcGetEventHandle
#endif
#ifndef cudaIpcOpenEventHandle
#define cudaIpcOpenEventHandle hipIpcOpenEventHandle
#endif
#ifndef cudaMalloc
#define cudaMalloc hipMalloc
#endif
#ifndef cudaMallocAsync
#define cudaMallocAsync hipMallocAsync
#endif
#ifndef cudaFree
#define cudaFree hipFree
#endif
#ifndef cudaMemcpy
#define cudaMemcpy hipMemcpy
#endif
#ifndef cudaMemcpyAsync
#define cudaMemcpyAsync hipMemcpyAsync
#endif
#ifndef cudaMemcpyPeerAsync
#define cudaMemcpyPeerAsync hipMemcpyPeerAsync
#endif
#ifndef cudaMemset
#define cudaMemset hipMemset
#endif
#ifndef cudaHostAlloc
#define cudaHostAlloc hipHostAlloc
#endif
#ifndef cudaFreeHost
#define cudaFreeHost hipHostFree
#endif
#ifndef cudaMemGetInfo
#define cudaMemGetInfo hipMemGetInfo
#endif
#ifndef cudaPointerGetAttributes
#define cudaPointerGetAttributes hipPointerGetAttributes
#endif
#ifndef cudaLaunchHostFunc
#define cudaLaunchHostFunc hipLaunchHostFunc
#endif

// ---- CUBLAS / CUBLASLt ----------------------------------------------------
#ifndef cublasHandle_t
#define cublasHandle_t hipblasHandle_t
#endif
#ifndef cublasLtHandle_t
#define cublasLtHandle_t hipblasLtHandle_t
#endif
