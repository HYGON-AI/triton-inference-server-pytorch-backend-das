// Copyright (c) 2026 Hygon Information Technology Co., Ltd.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
// HCU: map the CUDA runtime API onto HIP instead of using the DTK CUDA
// source-compatibility layer.
#include <hip/hip_runtime.h>
#include "cuda_to_hip.h"
