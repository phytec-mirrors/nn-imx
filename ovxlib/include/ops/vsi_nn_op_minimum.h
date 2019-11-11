/****************************************************************************
*
*    Copyright (c) 2019 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/
#ifndef _VSI_NN_OP_MINIMUM_H
#define _VSI_NN_OP_MINIMUM_H

#include "vsi_nn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VSI_NN_MINIMUM_SH_KERNEL_IDX(_INPUT0_TYPE, _INPUT1_TYPE, _OUTPUT_TYPE, _IMAGE_DIMS) \
    VSI_NN_MINIMUM_##_INPUT0_TYPE##_INPUT1_TYPE##TO##_OUTPUT_TYPE##_##_IMAGE_DIMS##_KERNEL,

enum {
    MINIMUM_CPU_KERNEL,

    VSI_NN_MINIMUM_SH_KERNEL_IDX(F16, F16, F16, IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I8,  F16, I8,  IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I8,  F16, F16, IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(U8,  F16, U8,  IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(U8,  F16, F16, IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I8,  I8,  I8,  IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(U8,  U8,  U8,  IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I16, I16, I16, IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I16, F16, I16, IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I16, F16, F16, IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(F16, F16, U8,  IMAGE_3D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(F16, F16, I8,  IMAGE_3D)

    VSI_NN_MINIMUM_SH_KERNEL_IDX(F16, F16, F16, IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I8,  F16, I8,  IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I8,  F16, F16, IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(U8,  F16, U8,  IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(U8,  F16, F16, IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I8,  I8,  I8,  IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(U8,  U8,  U8,  IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I16, I16, I16, IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I16, F16, I16, IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(I16, F16, F16, IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(F16, F16, U8,  IMAGE_2D)
    VSI_NN_MINIMUM_SH_KERNEL_IDX(F16, F16, I8,  IMAGE_2D)
};

enum {
    MINIMUM_INPUT0 = 0,
    MINIMUM_INPUT1,

    MINIMUM_INPUTS_COUNT,

    MINIMUM_OUTPUT = 0,

    MINIMUM_OUTPUTS_COUNT,

    MINIMUM_PARAM_COUT = MINIMUM_INPUTS_COUNT + MINIMUM_OUTPUTS_COUNT,
};

#define _VSI_NN_MINIMUM_LOCAL_TENSOR_NUM 3

typedef struct _vsi_nn_minimum_lcl_data
{
    vx_tensor   local_tensor[_VSI_NN_MINIMUM_LOCAL_TENSOR_NUM];
    uint32_t    hash_idx;
    vsi_bool    execute_on_sw;
    vsi_bool    enable_image_2d;
    uint32_t    sizes0[VSI_NN_MAX_DIM_NUM];
    uint32_t    sizes1[VSI_NN_MAX_DIM_NUM];
    uint32_t    sizes2[VSI_NN_MAX_DIM_NUM];
    uint32_t    dim_num;
} vsi_nn_minimum_lcl_data;

typedef struct _vsi_nn_minimum_param
{
    /* local data must be the first. */
    vsi_nn_minimum_lcl_data *local;
} vsi_nn_minimum_param;

#ifdef __cplusplus
}
#endif

#endif

