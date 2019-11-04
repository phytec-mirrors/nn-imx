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
#include <string.h>
#include <stdlib.h>

#include "vsi_nn_types.h"
#include "vsi_nn_platform.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_log.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_node.h"
#include "vsi_nn_ops.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "utils/vsi_nn_link_list.h"
#include "utils/vsi_nn_dtype_util.h"


static vx_int32 get_slice_axis_value(vx_int32 value, vx_uint32 dimension_size)
{
    vx_int32 axis_vlaue = 0;
    if (value < 0)
        axis_vlaue = value + dimension_size;
    else
        axis_vlaue = value;
    return axis_vlaue;
}

static vx_int32 get_slice_mask_start_value(vx_int32 stride, vx_uint32 dimension_size)
{
    vx_int32 start_vlaue = 0;
    if (stride > 0)
        start_vlaue = 0;
    else
        start_vlaue = dimension_size - 1;
    return start_vlaue;
}

static vx_int32 get_slice_mask_stop_value(vx_int32 stride, vx_uint32 dimension_size)
{
    vx_int32 stop_vlaue = 0;
    if (stride > 0)
        stop_vlaue = dimension_size;
    else
        stop_vlaue = -1;
    return stop_vlaue;
}

static vx_int32 get_slice_clamp_stop(vx_int32 stride, vx_int32 stop, vx_uint32 dimension_size)
{
    vx_int32 stop_vlaue = 0;
    if (stride > 0)
    {
        stop_vlaue = vsi_nn_clamp(stop, 0, (vx_int32)dimension_size);
    }
    else
    {
        stop_vlaue = vsi_nn_clamp(stop, -1, (vx_int32)dimension_size - 1);
    }
    return stop_vlaue;
}

static vsi_bool _get_stride_slice_start_stop_stride(
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    int32_t *start,
    int32_t *stop,
    int32_t *stride)
{
    vx_uint32 i = 0;
    vx_int32 int32_value = 0;
    vsi_nn_strided_slice_param *p = NULL;

    p = &(self->nn_param.strided_slice);

    for (i = 0; i < VSI_NN_MAX_DIM_NUM; i ++)
    {
        start[i]    = 0;
        stop[i]     = 1;
        stride[i]   = 1;
    }

    for (i = 0; i < p->stride_dims_num; ++i)
    {
        stride[i] = p->stride_dims[i];
    }

    for (i = 0; i < p->begin_dims_num; ++i)
    {
        int32_value = p->begin_dims[i];

        start[i] = get_slice_axis_value(int32_value, inputs[0]->attr.size[i]);
    }

    for (i = 0; i < p->end_dims_num; ++i)
    {
        int32_value = p->end_dims[i];

        stop[i] = get_slice_axis_value(int32_value, inputs[0]->attr.size[i]);
    }

    /*if the ith bit of mask is set, the start or stop will be the fullest possible range in that dimension.*/
    for (i = 0; i < inputs[0]->attr.dim_num; i ++)
    {
        if (p->begin_mask & (1 << i))
        {
            start[i] = get_slice_mask_start_value(stride[i], inputs[0]->attr.size[i]);
        }

        start[i] = vsi_nn_clamp(start[i], 0, (vx_int32)(inputs[0]->attr.size[i] - 1));

        if (p->shrink_axis_mask & (1 << i))
        {
            stop[i] = start[i] + 1;
        }

        if (p->end_mask & (1 << i))
        {
            stop[i] = get_slice_mask_stop_value(stride[i], inputs[0]->attr.size[i]);
        }

        stop[i] = get_slice_clamp_stop(stride[i], stop[i], inputs[0]->attr.size[i]);
    }

    return TRUE;
}

static vsi_bool _check_is_same_shape(
    vsi_nn_tensor_t ** inputs,
    int32_t *start,
    int32_t *stop,
    int32_t *stride
    )
{
    uint32_t i = 0;
    uint32_t dims = inputs[0]->attr.dim_num;

    for (i = dims - 1; i >= 0; i --)
    {
        if (inputs[0]->attr.size[i] == 1)
        {
            dims --;
            continue;
        }
        else
            break;
    }

    for (i = 0; i < dims - 1; i++)
    {
        if (stride[i] != 1 || start[i] != 0 || stop[i] != inputs[0]->attr.size[i])
            return FALSE;
    }

    if (stride[i] != 1)
        return FALSE;

    return TRUE;
}

static vsi_bool _is_same_quant
    (
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_nn_dtype_t *src_dtype = NULL,*dst_dtype = NULL;

    src_dtype = &inputs[0]->attr.dtype;
    dst_dtype = &outputs[0]->attr.dtype;

    if(vsi_nn_DtypeCompare(src_dtype, dst_dtype) == FALSE)
    {
        return FALSE;
    }

    return TRUE;
} /* _is_same_quant */


static vsi_status copy_tensor_to_view
    (
    vsi_nn_node_t   * self,
    vx_tensor         src_tensor,
    vsi_nn_tensor_t * dst_in
    )
{
    vsi_status ret;
    vsi_nn_strided_slice_lcl_data2 * data = NULL;

    ret = VSI_SUCCESS;
    /* Malloc ptr */
    data = (vsi_nn_strided_slice_lcl_data2 *)malloc( sizeof(vsi_nn_strided_slice_lcl_data2) );
    if( NULL == data )
    {
        VSILOGE( "Create stride slice local data fail." );
        return VSI_FAILURE;
    }
    memset( data, 0, sizeof(vsi_nn_concat_lcl_data) );
    data->src_tensor = src_tensor;
    if (dst_in->t)
        data->dst_tensor = vxReshapeTensor(dst_in->t, (int32_t*)dst_in->attr.size, dst_in->attr.dim_num);

    /* Store node, ptr */
    vsi_nn_LinkListPushStart(
        (vsi_nn_link_list_t **)&self->nn_param.strided_slice.lcl2_data,
        (vsi_nn_link_list_t *)data );

    return ret;
} /* copy_tensor_to_view() */

static vsi_status op_compute
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_status status = VSI_SUCCESS;
    vx_nn_stride_slice_params_t param;
    vsi_nn_tensor_t *begin_dims_tensor = NULL;
    vsi_nn_tensor_t *end_dims_tensor = NULL;
    vsi_nn_tensor_t *stride_dims_tensor = NULL;
    vsi_nn_tensor_attr_t attr;
    int32_t        start_dims[VSI_NN_MAX_DIM_NUM] = { 0 };
    int32_t        stop_dims[VSI_NN_MAX_DIM_NUM] = { 0 };
    int32_t        stride_dims[VSI_NN_MAX_DIM_NUM] = { 0 };
    vsi_nn_strided_slice_lcl_data2 * iter = NULL;

    _get_stride_slice_start_stop_stride(self, inputs, start_dims, stop_dims, stride_dims);

    if (_check_is_same_shape(inputs, start_dims, stop_dims, stride_dims) == vx_true_e)
    {
        iter = self->nn_param.strided_slice.lcl2_data;
        while( NULL != iter )
        {
            vx_tensor dst_tensor = iter->dst_tensor ? iter->dst_tensor : outputs[0]->t;
            iter->cp_node = vxTensorCopyNode(self->graph->g,
                iter->src_tensor, dst_tensor );
            if( NULL == iter->cp_node )
            {
                VSILOGE( "Create vxTensorCopyNode fail." );
                status = VSI_FAILURE;
                break;
            }
            iter = (vsi_nn_strided_slice_lcl_data2 *)vsi_nn_LinkListNext( (vsi_nn_link_list_t *)iter );
        }
    }
    else
    {
        memset(&param, 0, sizeof(vx_nn_stride_slice_params_t));

        memset(&attr, 0, sizeof(attr));
        attr.size[0] = self->nn_param.strided_slice.begin_dims_num;
        attr.dim_num = 1;
        attr.is_const = TRUE;
        attr.dtype.vx_type = VSI_NN_TYPE_INT32;
        attr.dtype.qnt_type = VSI_NN_QNT_TYPE_NONE;
        begin_dims_tensor = vsi_nn_CreateTensorFromData(
            self->graph,
            (uint8_t *)start_dims,
            &attr);
        if( NULL == begin_dims_tensor )
        {
            VSILOGE("Create begin_dims_tensor fail.(strided_slice)");
            return VSI_FAILURE;
        }

        self->nn_param.strided_slice.local.begin_dims_tensor = begin_dims_tensor;
        param.begin_dims = REQUIRED_IO(begin_dims_tensor);

        memset(&attr, 0, sizeof(attr));
        attr.size[0] = self->nn_param.strided_slice.end_dims_num;
        attr.dim_num = 1;
        attr.is_const = TRUE;
        attr.dtype.vx_type = VSI_NN_TYPE_INT32;
        attr.dtype.qnt_type = VSI_NN_QNT_TYPE_NONE;
        end_dims_tensor = vsi_nn_CreateTensorFromData(
            self->graph,
            (uint8_t *)stop_dims,
            &attr);
        if( NULL == end_dims_tensor )
        {
            VSILOGE("Create end_dims_tensor fail.(strided_slice)");
            return VSI_FAILURE;
        }

        self->nn_param.strided_slice.local.end_dims_tensor = end_dims_tensor;
        param.end_dims = REQUIRED_IO(end_dims_tensor);

        memset(&attr, 0, sizeof(attr));
        attr.size[0] = self->nn_param.strided_slice.stride_dims_num;
        attr.dim_num = 1;
        attr.is_const = TRUE;
        attr.dtype.vx_type = VSI_NN_TYPE_INT32;
        attr.dtype.qnt_type = VSI_NN_QNT_TYPE_NONE;
        stride_dims_tensor = vsi_nn_CreateTensorFromData(
            self->graph,
            (uint8_t *)stride_dims,
            &attr);
        if( NULL == stride_dims_tensor )
        {
            VSILOGE("Create stride_dims_tensor fail.(strided_slice)");
            return VSI_FAILURE;
        }

        self->nn_param.strided_slice.local.stride_dims_tensor = stride_dims_tensor;
        param.stride_dims = REQUIRED_IO(stride_dims_tensor);

        param.begin_mask = 0;
        param.end_mask = 0;
        param.shrink_axis_mask = 0;

        self->n = vxTensorStrideSliceNode(
            self->graph->g,
            inputs[0]->t,
            &param,
            sizeof(vx_nn_stride_slice_params_t),
            outputs[0]->t
            );

        if( NULL != self->n )
        {
            status = VSI_SUCCESS;
        }
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
    //TODO: Check tensor shapes.
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
    if( VSI_NN_DIM_AUTO == outputs[0]->attr.dim_num )
    {
        vsi_nn_strided_slice_param *p = &(self->nn_param.strided_slice);
        vx_uint32 i;
        for (i = 0; i < inputs[0]->attr.dim_num; i++)
        {
            vx_int32 begin = 0, end = 1, stride = 1;
            vx_int32 input_size = inputs[0]->attr.size[i];
            vx_int32 output_size = 0;
            vx_int32 j;

            begin = get_slice_axis_value(p->begin_dims[i], input_size);
            end = get_slice_axis_value(p->end_dims[i], input_size);
            stride = p->stride_dims[i];
            if (p->begin_mask & (1 << i))
            {
                begin = get_slice_mask_start_value(stride, input_size);
            }
            begin = vsi_nn_clamp(begin, 0, (vx_int32)(input_size - 1));
            if (p->shrink_axis_mask & (1 << i))
            {
                end = begin + 1;
            }

            if (p->end_mask & (1 << i))
            {
                end = get_slice_mask_stop_value(stride, input_size);
            }
            end = get_slice_clamp_stop(stride, end, input_size);
            for (j = begin; !((stride > 0) ? (j >= end) : (j <= end)); j += stride)
            {
                output_size++;
            }
            outputs[0]->attr.size[i] = output_size;
        }
        outputs[0]->attr.dim_num = 0;
        for (i = 0; i < inputs[0]->attr.dim_num; i++)
        {
            if (p->shrink_axis_mask & (1 << i)) continue;
            outputs[0]->attr.size[outputs[0]->
                attr.dim_num] = outputs[0]->attr.size[i];
            outputs[0]->attr.dim_num++;
        }
    }
    return vx_true_e;
} /* op_setup() */

static vsi_status op_optimize
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs,
    vsi_nn_opt_direction_e direction
    )
{
    vsi_status     status = VSI_SUCCESS;
    int32_t        i = 0;
    vx_tensor      in_view_tensor = NULL;
    uint32_t       start[VSI_NN_MAX_DIM_NUM] = { 0 };
    uint32_t       end[VSI_NN_MAX_DIM_NUM] = { 0 };
    int32_t        start_dims[VSI_NN_MAX_DIM_NUM] = { 0 };
    int32_t        stop_dims[VSI_NN_MAX_DIM_NUM] = { 0 };
    int32_t        stride_dims[VSI_NN_MAX_DIM_NUM] = { 0 };
    vsi_bool       is_same_quant_type = FALSE;

    /* Only forward run stride_slice's optimize */
    if( direction == VSI_NN_OPTIMIZE_BACKWARD )
    {
        return status;
    }

    _get_stride_slice_start_stop_stride(self, inputs, start_dims, stop_dims, stride_dims);

    if (_check_is_same_shape(inputs, start_dims, stop_dims, stride_dims) == vx_false_e)
        return status;

    VSILOGD("Optimize %s, uid %u", vsi_nn_OpGetName(self->op), self->uid);

    if( NULL == inputs[0]->t )
    {
        vsi_nn_TensorReinit( self->graph, inputs[0] );
    }

    /* Create tensor from view */
    memcpy( start, (uint32_t*)start_dims, sizeof( uint32_t ) * VSI_NN_MAX_DIM_NUM );
    memcpy( end, (uint32_t*)stop_dims, sizeof( uint32_t ) * VSI_NN_MAX_DIM_NUM );
    in_view_tensor = vsi_nn_CreateViewTensor(self->graph, start, end, inputs[0]);
    if( NULL == in_view_tensor )
    {
        VSILOGE( "Create tensor %d from view fail.", i );
        status = VSI_FAILURE;
        goto OnError;
    }

    is_same_quant_type = _is_same_quant(inputs, outputs);
    if( NULL != outputs[0]->t || is_same_quant_type == FALSE)
    {
        VSILOGW( "stride slice copy tensor.");
        // Copy old tensor values to the new address.
        status = copy_tensor_to_view( self, in_view_tensor, outputs[0]);
        if( VSI_FAILURE == status )
        {
            goto OnError;
        }
    }
    else
    {
        outputs[0]->t = in_view_tensor;
    }

OnError:
    return status;
} /* op_optimize() */

static vsi_status op_deinit
    (
    vsi_nn_node_t * self
    )
{
    vsi_nn_strided_slice_lcl_data2 * data;
    vsi_nn_strided_slice_lcl_data2 * tmp;

    if(NULL == self)
    {
        return VSI_FAILURE;
    }

    data = self->nn_param.strided_slice.lcl2_data;
    if(self->n)
    {
        if( NULL != self && NULL != self->n )
        {
            if(data)
            {
                free(data);
                data = NULL;
            }
            vxReleaseNode( &self->n );
            self->n = NULL;
        }
    }
    else
    {
        while( NULL != data )
        {
            tmp = (vsi_nn_strided_slice_lcl_data2 *)vsi_nn_LinkListPopStart(
                (vsi_nn_link_list_t **)&data );
            vxReleaseNode( &tmp->cp_node );
            vxReleaseTensor( &tmp->dst_tensor );
            free( tmp );
        }
    }

    if (self->nn_param.strided_slice.local.begin_dims_tensor != NULL)
    {
        vsi_nn_ReleaseTensor(&(self->nn_param.strided_slice.local.begin_dims_tensor));
    }
    if (self->nn_param.strided_slice.local.end_dims_tensor != NULL)
    {
        vsi_nn_ReleaseTensor(&(self->nn_param.strided_slice.local.end_dims_tensor));
    }
    if (self->nn_param.strided_slice.local.stride_dims_tensor != NULL)
    {
        vsi_nn_ReleaseTensor(&(self->nn_param.strided_slice.local.stride_dims_tensor));
    }
    vsi_nn_op_common_deinit(self);

    return VSI_SUCCESS;
} /* op_deinit() */

#ifdef __cplusplus
extern "C" {
#endif
/* Registrar */
DEF_OP_REG
    (
    /* op_name    */ STRIDED_SLICE,
    /* init       */ NULL,
    /* compute    */ op_compute,
    /* deinit     */ op_deinit,
    /* check      */ op_check,
    /* setup      */ op_setup,
    /* optimize   */ op_optimize,
    /* input_num  */ 1,
    /* output_num */ 1
    );
#ifdef __cplusplus
}
#endif

