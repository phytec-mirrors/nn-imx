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
#include "vsi_nn_platform.h"
#include "vsi_nn_types.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_log.h"

vsi_status vsi_nn_softmax_compute
    (
    vsi_nn_node_t * self,
    vsi_nn_tensor_t ** inputs,
    vsi_nn_tensor_t ** outputs
    )
{
    vsi_status status;
    vsi_nn_softmax_internal_lcl_data * iter;
    uint32_t n;

    n = 0;
    status = VSI_FAILURE;
    iter = self->nn_param.softmax_internal.data;
    self->n = NULL;
    if(NULL != iter)
    {
        while (iter)
        {
            iter->node = vxSoftmaxLayer(self->graph->g,
                iter->src_tensor, iter->dst_tensor);
            if(iter->node == NULL)
            {
                VSILOGE( "Create vxSoftmaxLayer fail." );
                status = VSI_FAILURE;
                break;
            }
            iter = (vsi_nn_softmax_internal_lcl_data *)vsi_nn_LinkListNext((vsi_nn_link_list_t *)iter);
            n++;
            status = VSI_SUCCESS;
        }
        VSILOGD("Create %u vx softmax layer", n);
    }
    else
    {
        self->n = vxSoftmaxLayer( self->graph->g,
            inputs[0]->t, outputs[0]->t );
        if( NULL != self->n )
        {
            status = VSI_SUCCESS;
        }
    }

    return status;
} /* vsi_nn_softmax_compute() */