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
#ifndef _VSI_NN_OP_RESIZE_NEAREST_INTERNAL_H
#define _VSI_NN_OP_RESIZE_NEAREST_INTERNAL_H

#include "vsi_nn_types.h"

#define VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(SRC_TYPE, DST_TYPE, INSTR) \
    VSI_NN_NEATEST_INTERNAL_##SRC_TYPE##TO##DST_TYPE##_##INSTR##_KERNEL,


enum {
    NEAREST_INTERNAL_CPU_KERNEL,
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(F16, F16, LARGE)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(I16, I16, LARGE)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(I8, I8, LARGE)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(U8, U8, LARGE)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(F16, F16, SMALL)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(I16, I16, SMALL)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(I8, I8, SMALL)
    VSI_NN_NEAREST_INTERNAL_SH_KERNEL_IDX(U8, U8, SMALL)
    NEAREST_INTERNAL_KERNEL_COUNTS,
};

typedef struct _vsi_nn_resize_nearest_in_lcl_data
{
    uint32_t    hash_idx;
    vsi_bool    execute_on_sw;
} vsi_nn_resize_nearest_in_lcl_data;

typedef struct _vsi_nn_resize_nearest_internal_param
{
    vsi_nn_resize_nearest_in_lcl_data *lcl_data_ptr;
    vx_bool_e    align_corners;
    vx_bool_e    half_pixel_centers;
    float        factor;
} vsi_nn_resize_nearest_internal_param;


#endif

