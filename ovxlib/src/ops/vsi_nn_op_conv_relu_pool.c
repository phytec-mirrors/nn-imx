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
#include "vsi_nn_types.h"
#include "vsi_nn_platform.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_log.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_node.h"
#include "vsi_nn_ops.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_tensor_util.h"
#include "ops/vsi_nn_op_conv_relu.h"
#include "utils/vsi_nn_util.h"
#include "utils/vsi_nn_dtype_util.h"

static vsi_status op_compute
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_status status;
    vx_nn_convolution_relu_pooling_params_ext2_t p;
    status = VSI_FAILURE;

    if(vsi_nn_InitConvReluPoolParameter(self, &p, TRUE) != VSI_SUCCESS)
    {
        VSILOGE("SetConvReluPoolParameter fail\n");
        return VSI_FAILURE;
    }

    self->n = vxConvolutionReluPoolingLayer2(
        self->graph->g,
        inputs[0]->t,
        inputs[1]->wb,
        (vx_nn_convolution_relu_pooling_params_t *)&p,
        sizeof(p),
        outputs[0]->t
        );

    vsi_nn_DeinitConvReluPoolParameter( &p );

    if( NULL != self->n )
    {
        status = VSI_SUCCESS;
    }
    return status;
} /* op_compute() */

static vsi_bool op_check
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_bool ret = FALSE;

    /* Check fl and scale*/
    ret = vsi_nn_QuantCheck(inputs[0], inputs[1], inputs[2]);

    return ret;
} /* op_check() */

static vsi_bool op_setup
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_bool ret;

#ifdef VX_CONVERT_POLICY_WRAP_ENABLE
    if ( vsi_nn_compareVersion(self->graph, 1, 1, 21) == -1 )
    {
        self->vx_param.overflow_policy = VX_CONVERT_POLICY_SATURATE;
    }
#endif

    ret = TRUE;
    if( VSI_NN_DIM_AUTO == outputs[0]->attr.dim_num )
    {
        ret = vsi_nn_OpSetup( VSI_NN_OP_CONV2D, self, inputs, outputs );
        if(ret == FALSE)
        {
            VSILOGE("OpSetup [VSI_NN_OP_CONV2D] fail\n");
            return FALSE;
        }

        ret = vsi_nn_OpSetup( VSI_NN_OP_POOL, self, outputs, outputs );
        if(ret == FALSE)
        {
            VSILOGE("OpSetup [VSI_NN_OP_POOL] fail\n");
            return FALSE;
        }
    }

    return ret;
} /* op_setup() */

static vsi_status op_optimize
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs,
    vsi_nn_opt_direction_e direction
    )
{
    vsi_status status;
    vsi_bool ret;
    vsi_nn_tensor_t conv_out, *pconv_out;
    vx_nn_convolution_relu_pooling_params_ext2_t p;
    vx_weights_biases_parameter_optimizations_t opt;
    vx_weights_biases_parameter_optimizations_t * p_opt;

    ret = FALSE;
    status = VSI_FAILURE;

    if(direction == VSI_NN_OPTIMIZE_BACKWARD)
    {
        return VSI_SUCCESS;
    }

    VSILOGD("Optimize %s", vsi_nn_OpGetName(self->op));
    memset(&conv_out, 0, sizeof(vsi_nn_tensor_t));
    pconv_out = &conv_out;

    ret = vsi_nn_OpSetup( VSI_NN_OP_CONV2D, self, inputs, &pconv_out );
    if(ret == FALSE)
    {
        VSILOGE("OpSetup [VSI_NN_OP_CONV2D] fail\n");
        goto final;
    }

    /* Prepare weight_bias */
    if(inputs[1]->wb == NULL)
    {
        if(vsi_nn_InitConvReluPoolParameter(self, &p, TRUE) != VSI_SUCCESS)
        {
            VSILOGE("SetConvReluPoolParameter fail\n");
            goto final;
        }

        p_opt = NULL;
        if( outputs[0]->attr.dtype.qnt_type == VSI_NN_QNT_TYPE_AFFINE_ASYMMETRIC
         || inputs[0]->attr.dtype.qnt_type == VSI_NN_QNT_TYPE_AFFINE_ASYMMETRIC)
        {
            memset( &opt, 0, sizeof( opt ) );
            opt.inputZeroPoint = inputs[0]->attr.dtype.zero_point;
            opt.zrl = -1;
            opt.outputFormat = outputs[0]->attr.dtype.vx_type;
            p_opt = &opt;
        }

        inputs[1]->wb = vxCreateWeightsBiasesParameterFromTensors2(
            VX_CONVOLUTIONAL_NETWORK_CONVOLUTION_LAYER,
            4,
            inputs[0]->attr.size,
            pconv_out->attr.size,
            outputs[0]->attr.size,
            outputs[0]->attr.dtype.vx_type,
            (vx_nn_convolution_relu_pooling_params_t *)&p,
            sizeof(p),
            p_opt,
            inputs[1]->t, inputs[2]->t
            );
        vsi_nn_DeinitConvReluPoolParameter( &p );
    }

    if( NULL == inputs[1]->wb )
    {
        VSILOGE( "Create weight bias fail." );
    }
    else
    {
        status = VSI_SUCCESS;
    }

final:
    return status;
} /* op_optimize() */

#ifdef __cplusplus
extern "C" {
#endif
/* Registrar */
DEF_OP_REG
    (
    /* op_name    */ CONV_RELU_POOL,
    /* init       */ NULL,
    /* compute    */ op_compute,
    /* deinit     */ vsi_nn_op_common_deinit,
    /* check      */ op_check,
    /* setup      */ op_setup,
    /* optimize   */ op_optimize,
    /* input_num  */ 3,
    /* output_num */ 1
    );
#ifdef __cplusplus
}
#endif

