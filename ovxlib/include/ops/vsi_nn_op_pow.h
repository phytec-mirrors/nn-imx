/****************************************************************************
*
*    Copyright (c) 2018 Vivante Corporation
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
#ifndef _VSI_NN_OP_CLIENT_POW_H
#define _VSI_NN_OP_CLIENT_POW_H

#include "vsi_nn_types.h"
#include "vsi_nn_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    POWER_INPUT0, //optional
    POWER_INPUT1,

    POWER_INPUTS_COUNT,

    POWER_OUTPUT = 0,

    POWER_OUTPUTS_COUNT,

    POWER_PARAM_COUT = POWER_INPUTS_COUNT + POWER_OUTPUTS_COUNT,
};

#define _VSI_NN_POW_LOCAL_TENSOR_NUM 3

typedef struct _vsi_nn_pow_lcl_data
{
    vx_tensor   local_tensor[_VSI_NN_POW_LOCAL_TENSOR_NUM];
} vsi_nn_pow_lcl_data;

typedef struct _vsi_nn_pow_param
{
    /* local data must be the first. */
    vsi_nn_pow_lcl_data local;
} vsi_nn_pow_param;

#ifdef __cplusplus
}
#endif

#endif
