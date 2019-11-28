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
#ifndef _VSI_NN_OP_SIN_H
#define _VSI_NN_OP_SIN_H

#include "vsi_nn_types.h"


enum {
    TENSOR_SIN_INPUT,

    TENSOR_SIN_INPUTS_COUNT,

    TENSOR_SIN_OUTPUT = 0,

    TENSOR_SIN_OUTUTS_COUNT,

    TENSOR_SIN_PARAM_COUT = TENSOR_SIN_INPUTS_COUNT + TENSOR_SIN_OUTUTS_COUNT,
};

enum {
    TENSOR_SIN_CPU_KERNEL,

    TENSOR_SIN_F16TOF16_KERNEL,
    TENSOR_SIN_F16TOI16_KERNEL,
    TENSOR_SIN_F16TOI8_KERNEL,
    TENSOR_SIN_F16TOU8_KERNEL,
    TENSOR_SIN_I16TOI16_KERNEL,
    TENSOR_SIN_I16TOF16_KERNEL,
    TENSOR_SIN_I8TOI8_KERNEL,
    TENSOR_SIN_I8TOF16_KERNEL,
    TENSOR_SIN_U8TOU8_KERNEL,
    TENSOR_SIN_U8TOF16_KERNEL,

    TENSOR_SIN_F16TOF16_2D_KERNEL,
    TENSOR_SIN_F16TOI16_2D_KERNEL,
    TENSOR_SIN_F16TOI8_2D_KERNEL,
    TENSOR_SIN_F16TOU8_2D_KERNEL,
    TENSOR_SIN_I16TOI16_2D_KERNEL,
    TENSOR_SIN_I16TOF16_2D_KERNEL,
    TENSOR_SIN_I8TOI8_2D_KERNEL,
    TENSOR_SIN_I8TOF16_2D_KERNEL,
    TENSOR_SIN_U8TOU8_2D_KERNEL,
    TENSOR_SIN_U8TOF16_2D_KERNEL,

    TENSOR_SIN_KERNEL_COUNTS,
};

#define _VSI_NN_SIN_LOCAL_TENSOR_NUM 2

typedef struct _vsi_nn_sin_lcl_data
{
    vx_tensor   local_tensor[_VSI_NN_SIN_LOCAL_TENSOR_NUM];
} vsi_nn_sin_lcl_data;

typedef struct _vsi_nn_sin_param
{
    /* exp layer local data structure */
    vsi_nn_sin_lcl_data local;
} vsi_nn_sin_param;

#endif
