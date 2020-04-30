/****************************************************************************
*
*    Copyright (c) 2020 Vivante Corporation
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


#include <string.h>
#include <stdlib.h>

#include "vsi_nn_types.h"
#include "vsi_nn_log.h"
#include "vsi_nn_node.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_ops.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "kernel/vsi_nn_kernel.h"
#include "kernel/vsi_nn_kernel_eltwise.h"

/*
 Declare number of input and output.
 */
#define _INPUT_NUM          (5)
#define _OUTPUT_NUM         (1)

static vsi_status op_compute
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_param_t * param = NULL;
    int32_t  shapes[4][VSI_NN_MAX_DIM_NUM] = {{ 1 }};
    int32_t* shapes_ptr[4] = {NULL};
    int32_t  *shapes_in[3] = {NULL};
    uint32_t rank_in[3] = {0};
    uint32_t new_rank = 0;
    vsi_nn_tensor_t* reshape_tensors[6] = { NULL };
    vsi_bool ret = TRUE;
    uint32_t i = 0;

    param = vsi_nn_kernel_param_create();
    vsi_nn_kernel_param_add_float32( param, "eps", self->nn_param.batch_norm.eps );

    rank_in[0] = inputs[0]->attr.dim_num;
    rank_in[1] = inputs[1]->attr.dim_num;
    rank_in[2] = inputs[3]->attr.dim_num;
    shapes_in[0] = (int32_t *)inputs[0]->attr.size;
    shapes_in[1] = (int32_t *)inputs[1]->attr.size;
    shapes_in[2] = (int32_t *)inputs[3]->attr.size;
    for (i = 0; i < 4; i++)
    {
        shapes_ptr[i] = shapes[i];
    }

    ret = vsi_nn_kernel_optimize_broadcast_shape(
            (const int32_t**)shapes_in, (const size_t*)rank_in, 3,
            (int32_t *)outputs[0]->attr.size, outputs[0]->attr.dim_num,
            shapes_ptr, shapes[3], &new_rank);

    if( ret )
    {
        reshape_tensors[0] = vsi_nn_reshape_tensor( self->graph,
            inputs[0], (uint32_t*)shapes[0], new_rank );
        reshape_tensors[1] = vsi_nn_reshape_tensor( self->graph,
            inputs[1], (uint32_t*)shapes[1], new_rank );
        reshape_tensors[2] = vsi_nn_reshape_tensor( self->graph,
            inputs[2], (uint32_t*)shapes[1], new_rank );
        reshape_tensors[3] = vsi_nn_reshape_tensor( self->graph,
            inputs[3], (uint32_t*)shapes[2], new_rank );
        reshape_tensors[4] = vsi_nn_reshape_tensor( self->graph,
            inputs[4], (uint32_t*)shapes[2], new_rank );

        reshape_tensors[5] = vsi_nn_reshape_tensor( self->graph,
            outputs[0], (uint32_t*)shapes[3], new_rank );
    }
    else
    {
        reshape_tensors[0] = inputs[0];
        reshape_tensors[1] = inputs[1];
        reshape_tensors[2] = inputs[2];
        reshape_tensors[3] = inputs[3];
        reshape_tensors[4] = inputs[4];

        reshape_tensors[5] = outputs[0];
    }

    self->n = (vx_node)vsi_nn_kernel_selector( self->graph,
        "batchnorm_single",
        reshape_tensors, 5,
        &reshape_tensors[5], 1, param );

    if( self->n )
    {
        status = VSI_SUCCESS;
    }

    if (ret)
    {
        for ( i = 0; i < 6; i++)
        {
            if (reshape_tensors[i])
            {
                vsi_nn_ReleaseTensor( &reshape_tensors[i] );
            }
        }
    }

    vsi_nn_kernel_param_release( &param );

    return status;
} /* op_compute() */

static vsi_bool op_check
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    /*TODO: Check tensor shapes. */
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t rank = inputs[0]->attr.dim_num;

    for(i = 0; i < rank; i++)
    {
        vx_int32 shape0 = inputs[0]->attr.size[i];

        for ( j = 1; j < self->input.num; j++)
        {
            uint32_t rank1 = inputs[j]->attr.dim_num;
            vx_int32 shape1 = rank1 > i ? inputs[j]->attr.size[i] : 1;

            if(shape0 != shape1 && shape1 != 1)
            {
                VSILOGE("Invalid broadcast for inputs[%d] size[%u]", j, shape1);
                return FALSE;
            }
        }
    }

    return TRUE;
} /* op_check() */

static vsi_bool op_setup
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    /* TODO: Add code to comput outputs' shape. */
    return TRUE;
} /* op_setup() */

__BEGIN_DECLS

/* Registrar */
DEF_OP_REG
    (
    /* op_name    */ BATCHNORM_SINGLE,
    /* init       */ NULL,
    /* compute    */ op_compute,
    /* deinit     */ vsi_nn_op_common_deinit,
    /* check      */ op_check,
    /* setup      */ op_setup,
    /* optimize   */ NULL,
    /* input_num  */ _INPUT_NUM,
    /* output_num */ _OUTPUT_NUM
    );

__END_DECLS

