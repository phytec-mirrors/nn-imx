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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "vsi_nn_platform.h"

#include "vsi_nn_prv.h"
#include "vsi_nn_log.h"
#include "vsi_nn_test.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "utils/vsi_nn_dtype_util.h"
#include "utils/vsi_nn_math.h"
#include "client/vsi_nn_vxkernel.h"
#include "libnnext/vx_lib_nnext.h"

#define _VX_KERNEL_VAR          (vx_kernel_TOPK)
#define _VX_KERNEL_ID           (VX_KERNEL_ENUM_TOPK)
#define _VX_KERNEL_NAME         (VX_KERNEL_NAME_TOPK)
#define _VX_KERNEL_FUNC_KERNEL  (vxTopkKernel)

static int32_t partition
(
    float* input,
    int32_t left,
    int32_t right,
    vsi_bool is_recursion,
    uint32_t* indices
)
{
    float key;
    int32_t key_index;
    int32_t low = left;
    int32_t high = right;
    if (left < right)
    {
        key_index = indices[left];
        key = input[key_index];
        while (low < high)
        {
            while (low < high && input[indices[high]] <= key)
            {
                high--;
            }
            indices[low] = indices[high];
            while (low < high && input[indices[low]] >= key)
            {
                low++;
            }
            indices[high] = indices[low];
        }
        indices[low] = key_index;
        if (is_recursion)
        {
            partition(input, left, low - 1, TRUE, indices);
            partition(input, low + 1, right, TRUE, indices);
        }
    }
    return low;
}

static void find_top_k_1d
(
    float* input,
    uint32_t input_len,
    uint32_t k,
    float* value,
    uint32_t* indices
)
{
    int32_t low = 0;
    int32_t high = input_len - 1;
    int32_t j;

    for (j = 0; j < (int32_t)input_len; j++)
    {
        indices[j] = j;
    }

    j = partition(input, low, high, FALSE, indices);

    //part_sort
    while (j != k)
    {
        if ((int32_t)k > j)
        {
            low = j + 1;
        }
        else
        {
            high = j;
        }
        j = partition(input, low, high, FALSE, indices);
    }
    //all_sort
    partition(input, 0, k - 1, TRUE, indices);

    for (j = 0; j < (int32_t)k; j++)
    {
        value[j] = input[indices[j]];
    }
}

static vsi_status VX_CALLBACK vxTopkKernel
    (
    vx_node node,
    const vx_reference* paramObj,
    uint32_t paramNum
    )
{
#define ARG_NUM            (1)
#define TENSOR_NUM_INPUT (1)
#define TENSOR_NUM_OUTPUT (2)
#define TENSOR_NUM (TENSOR_NUM_INPUT+TENSOR_NUM_OUTPUT)

    vsi_status status = VSI_FAILURE;
    vx_context context = NULL;
    vx_tensor input[TENSOR_NUM_INPUT] = {0};
    vx_tensor output[TENSOR_NUM_OUTPUT] = {0};
    float *f32_in_buffer[TENSOR_NUM_INPUT] = {0};
    float *f32_out_buffer = NULL;
    uint32_t *u32_out_buffer = NULL;
    vsi_nn_tensor_attr_t in_attr[TENSOR_NUM_INPUT] = {0};
    vsi_nn_tensor_attr_t out_attr[TENSOR_NUM_OUTPUT] = {0};
    uint32_t in_elements[TENSOR_NUM_INPUT] = {0};
    uint32_t out_elements[TENSOR_NUM_OUTPUT]= {0};

    int32_t top_k;

    int32_t i;

    /* prepare data */
    context = vxGetContext((vx_reference)node);

    for(i = 0; i < TENSOR_NUM_INPUT; i ++)
    {
        input[i] = (vx_tensor)paramObj[i];
        status = vsi_nn_vxGetTensorAttr(input[i], &in_attr[i]);
        TEST_CHECK_STATUS(status, final);
        in_elements[i] = vsi_nn_vxGetTensorElementNum(&in_attr[i]);
        f32_in_buffer[i] = (float *)malloc(in_elements[i] * sizeof(float));
        status = vsi_nn_vxConvertTensorToFloat32Data(
            context, input[i], &in_attr[i], f32_in_buffer[i],
            in_elements[i] * sizeof(float));
        TEST_CHECK_STATUS(status, final);
    }
    for(i = 0; i < TENSOR_NUM_OUTPUT; i ++)
    {
        output[i] = (vx_tensor)paramObj[i + TENSOR_NUM_INPUT];
        status = vsi_nn_vxGetTensorAttr(output[i], &out_attr[i]);
        TEST_CHECK_STATUS(status, final);
        out_elements[i] = vsi_nn_vxGetTensorElementNum(&out_attr[i]);
    }
    f32_out_buffer = (float *)malloc(out_elements[0] * sizeof(float));
    u32_out_buffer = (uint32_t *)malloc(out_elements[1] * sizeof(uint32_t));
    vxCopyScalar((vx_scalar)paramObj[TENSOR_NUM], &(top_k),
        VX_READ_ONLY, VX_MEMORY_TYPE_HOST);

    /* TODO: Add CPU kernel implement */
    /* example code : copy data form input tensor to output tensor*/
    {
        uint32_t block_num = 1;
        uint32_t block_size = in_attr[0].size[0];
        uint32_t i;
        uint32_t * indices = (uint32_t*)malloc(block_size * sizeof(uint32_t));

        for(i = 0; i < block_num; i++)
        {
            uint32_t in_index = i * block_size;
            uint32_t out_index = i * top_k;
            find_top_k_1d(&(f32_in_buffer[0][in_index * sizeof(float)]),
                block_size, top_k, &(f32_out_buffer[out_index * sizeof(float)]), indices);
            memcpy(&(u32_out_buffer[out_index * sizeof(uint32_t)]),
                indices, top_k * sizeof(uint32_t));
        }
        if (indices) free(indices);
    }
/*
    {
        uint32_t n, c, h, w;
        uint32_t batch = in_attr[0].size[3];
        uint32_t channel = in_attr[0].size[2];
        uint32_t height = in_attr[0].size[1];
        uint32_t width = in_attr[0].size[0];
        for(n = 0; n < batch; ++n)
        {
            for(c = 0; c < channel; ++c)
            {
                for(h = 0; h < height; ++h)
                {
                    for(w = 0; w < width; ++w)
                    {
                        float val;
                        uint32_t index = w + h * width + c * width * height
                            + n * width * height * channel;
                        f32_out_buffer[0][index] = f32_in_buffer[0][index];
                    }
                }
            }
        }
    }
*/

    /* save data */
    status = vsi_nn_vxConvertFloat32DataToTensor(
            context, output[0], &out_attr[0], f32_out_buffer,
            out_elements[0] * sizeof(float));
    TEST_CHECK_STATUS(status, final);
    vsi_nn_vxCopyDataToTensor(context, output[1], &out_attr[1], (uint8_t *)u32_out_buffer);

final:
    for (i = 0; i < TENSOR_NUM_INPUT; i++)
    {
        if (f32_in_buffer[i]) free(f32_in_buffer[i]);
    }
    if (f32_out_buffer) free(f32_out_buffer);
    if (u32_out_buffer) free(u32_out_buffer);
    return status;
} /* _VX_KERNEL_FUNC_KERNEL() */

static vx_param_description_t vxTopkKernelParam[] =
{
    {VX_INPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
};

vx_status VX_CALLBACK vxTopkInitializer
    (
    vx_node nodObj,
    const vx_reference *paramObj,
    vx_uint32 paraNum
    )
{
    vx_status status = VX_SUCCESS;
    /*TODO: Add initial code for VX program*/

    return status;
}


#ifdef __cplusplus
extern "C" {
#endif
vx_kernel_description_t vxTopk_CPU =
{
    _VX_KERNEL_ID,
    _VX_KERNEL_NAME,
    _VX_KERNEL_FUNC_KERNEL,
    vxTopkKernelParam,
    _cnt_of_array( vxTopkKernelParam ),
    vsi_nn_KernelValidator,
    NULL,
    NULL,
    vsi_nn_KernelInitializer,
    vsi_nn_KernelDeinitializer
};

vx_kernel_description_t vxTopk_VX =
{
    _VX_KERNEL_ID,
    _VX_KERNEL_NAME,
    NULL,
    vxTopkKernelParam,
    _cnt_of_array( vxTopkKernelParam ),
    vsi_nn_KernelValidator,
    NULL,
    NULL,
    vxTopkInitializer,
    vsi_nn_KernelDeinitializer
};

vx_kernel_description_t * vx_kernel_TOPK_list[] =
{
    &vxTopk_CPU,
    &vxTopk_VX,
    NULL
};
#ifdef __cplusplus
}
#endif
