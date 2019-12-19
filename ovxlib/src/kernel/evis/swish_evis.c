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


#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vsi_nn_types.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_log.h"
#include "vsi_nn_error.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_tensor_util.h"
#include "utils/vsi_nn_util.h"
#include "utils/vsi_nn_dtype_util.h"
#include "utils/vsi_nn_math.h"
#include "kernel/vsi_nn_kernel.h"
#include "libnnext/vx_lib_nnext.h"

__BEGIN_DECLS

typedef enum _internal_img_dim_e
{
    IMAGE = 0,
    IMAGE_2D,
} internal_img_dim_e;

#define _SWISH_KERNEL_SOURCE          "swish",
#define _HSWISH_KERNEL_SOURCE         "hswish",


#define STR(a) #a
// Add kernel hashtable here
#define SWISH_HASH_KEY(SWISH_TYPE, IN_DTYPE, OUT_DTYPE, _image_2d) \
        ((SWISH_TYPE << 20) | ( IN_DTYPE << 12 ) | ( OUT_DTYPE << 4) | (_image_2d))

#define SWISH_PACK_KERNEL_MAP( IN_DTYPE, OUT_DTYPE, INSTR) \
        { SWISH_HASH_KEY(VSI_NN_SWISH, IN_DTYPE, OUT_DTYPE, IMAGE##INSTR), \
        CVIVANTE_NAMESPACE("evis.swish_"STR(IN_DTYPE)"to"STR(OUT_DTYPE)STR(INSTR)), \
        _SWISH_KERNEL_SOURCE }

#define HSWISH_PACK_KERNEL_MAP( IN_DTYPE, OUT_DTYPE, INSTR) \
        { SWISH_HASH_KEY(VSI_NN_HSWISH, IN_DTYPE, OUT_DTYPE, IMAGE##INSTR), \
        CVIVANTE_NAMESPACE("evis.hswish_"STR(IN_DTYPE)"to"STR(OUT_DTYPE)STR(INSTR)), \
        _HSWISH_KERNEL_SOURCE }

typedef struct
{
    uint32_t key;
    char * function_name;
    const char * source_name;
} _kernel_map_type;

static const _kernel_map_type _swish_kernel_map[] =
{
    // Register kernel here
    SWISH_PACK_KERNEL_MAP(  F16,  F16,  ),
    SWISH_PACK_KERNEL_MAP(  F16,  I16,  ),
    SWISH_PACK_KERNEL_MAP(  F16,  I8,   ),
    SWISH_PACK_KERNEL_MAP(  F16,  U8,   ),
    SWISH_PACK_KERNEL_MAP(  I16,  I16,  ),
    SWISH_PACK_KERNEL_MAP(  I16,  F16,  ),
    SWISH_PACK_KERNEL_MAP(  I8,   I8,   ),
    SWISH_PACK_KERNEL_MAP(  I8,   F16,  ),
    SWISH_PACK_KERNEL_MAP(  U8,   U8,   ),
    SWISH_PACK_KERNEL_MAP(  U8,   F16,  ),
    SWISH_PACK_KERNEL_MAP(  BF16, BF16, ),
    SWISH_PACK_KERNEL_MAP(  F16,  F16,  _2D),
    SWISH_PACK_KERNEL_MAP(  F16,  I16,  _2D),
    SWISH_PACK_KERNEL_MAP(  F16,  I8,   _2D),
    SWISH_PACK_KERNEL_MAP(  F16,  U8,   _2D),
    SWISH_PACK_KERNEL_MAP(  I16,  I16,  _2D),
    SWISH_PACK_KERNEL_MAP(  I16,  F16,  _2D),
    SWISH_PACK_KERNEL_MAP(  I8,   I8,   _2D),
    SWISH_PACK_KERNEL_MAP(  I8,   F16,  _2D),
    SWISH_PACK_KERNEL_MAP(  U8,   U8,   _2D),
    SWISH_PACK_KERNEL_MAP(  U8,   F16,  _2D),
    SWISH_PACK_KERNEL_MAP(  BF16, BF16, _2D),
    HSWISH_PACK_KERNEL_MAP( F16,  F16,  ),
    HSWISH_PACK_KERNEL_MAP( F16,  I16,  ),
    HSWISH_PACK_KERNEL_MAP( F16,  I8,   ),
    HSWISH_PACK_KERNEL_MAP( F16,  U8,   ),
    HSWISH_PACK_KERNEL_MAP( I16,  I16,  ),
    HSWISH_PACK_KERNEL_MAP( I16,  F16,  ),
    HSWISH_PACK_KERNEL_MAP( I8,   I8,   ),
    HSWISH_PACK_KERNEL_MAP( I8,   F16,  ),
    HSWISH_PACK_KERNEL_MAP( U8,   U8,   ),
    HSWISH_PACK_KERNEL_MAP( U8,   F16,  ),
    HSWISH_PACK_KERNEL_MAP( BF16, BF16, ),
    HSWISH_PACK_KERNEL_MAP( F16,  F16,  _2D),
    HSWISH_PACK_KERNEL_MAP( F16,  I16,  _2D),
    HSWISH_PACK_KERNEL_MAP( F16,  I8,   _2D),
    HSWISH_PACK_KERNEL_MAP( F16,  U8,   _2D),
    HSWISH_PACK_KERNEL_MAP( I16,  I16,  _2D),
    HSWISH_PACK_KERNEL_MAP( I16,  F16,  _2D),
    HSWISH_PACK_KERNEL_MAP( I8,   I8,   _2D),
    HSWISH_PACK_KERNEL_MAP( I8,   F16,  _2D),
    HSWISH_PACK_KERNEL_MAP( U8,   U8,   _2D),
    HSWISH_PACK_KERNEL_MAP( U8,   F16,  _2D),
    HSWISH_PACK_KERNEL_MAP( BF16, BF16, _2D),
};


/*
 * Kernel params
 */
static vx_param_description_t _swish_kernel_param_def[] =
{
    {VX_INPUT,  VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_TENSOR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT,  VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
};
#define _SWISH_PARAM_NUM  _cnt_of_array( _swish_kernel_param_def )

/*
 * Kernel initializer
 */
static vx_status VX_CALLBACK _swish_initializer
    (
    vx_node              node,
    const vx_reference * param,
    uint32_t             param_size
    )
{
    vsi_status status = VX_SUCCESS;
    // Alignment with a power of two value.
    gpu_param_t gpu_param = {
        3,
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
        };

    vx_tensor     input            = (vx_tensor)param[0];
    vx_tensor     output           = (vx_tensor)param[1];
    int8_t        srcFixPointPos   = 0;
    int8_t        dstFixPointPos   = 0;
    vx_float32    inputTail        = 0;
    vx_float32    inputScale       = 1.0f;
    vx_float32    outputZP         = 0;
    vx_float32    outputScale      = 1.0f;
    vx_float32    logE             = (vx_float32)(log10(exp(1.0f)) / log10(2.0f));
    vsi_nn_kernel_tensor_attr_t *input_attr = NULL, *output_attr = NULL;
    vsi_int_array_t             *out_shape  = NULL;
    uint32_t                     pack_key   = 0;

    input_attr  = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)input);
    if (NULL == input_attr)
    {
        VSILOGE("vsi_nn_kernel_tensor_attr_create failure! at line %d\n", __LINE__);
        status = VX_ERROR_NO_MEMORY;
        goto final;
    }
    output_attr = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)output);
    if (status != VX_SUCCESS)
    {
        VSILOGE("vsi_nn_kernel_tensor_attr_create failure! at line %d\n", __LINE__);
        status = VX_ERROR_NO_MEMORY;
        goto final;
    }

    out_shape = output_attr->shape;

    if (input_attr->quant == VSI_NN_KERNEL_QUANT_DFP)
    {
        srcFixPointPos = input_attr->dfp.fl;
        if (srcFixPointPos > 0)
        {
            inputScale = 1.0f / (vx_float32) (1 << srcFixPointPos);
        }
        else
        {
            inputScale = (vx_float32)(1 << -srcFixPointPos);
        }
    }
    else if (input_attr->quant == VSI_NN_KERNEL_QUANT_ASYMM || input_attr->quant == VSI_NN_KERNEL_QUANT_SYMM)
    {
        inputScale  = input_attr->asymm.scale;
        inputTail   = 0 - input_attr->asymm.zero_point * inputScale;
    }

    if (output_attr->quant == VSI_NN_KERNEL_QUANT_DFP)
    {
        dstFixPointPos = output_attr->dfp.fl;
        if (dstFixPointPos > 0)
        {
            outputScale = (vx_float32) (1 << dstFixPointPos);
        }
        else
        {
            outputScale = 1.0f / (vx_float32) (1 << -dstFixPointPos);
        }
    }
    else if (output_attr->quant == VSI_NN_KERNEL_QUANT_ASYMM || output_attr->quant == VSI_NN_KERNEL_QUANT_SYMM)
    {
        outputScale  = 1.0 / output_attr->asymm.scale;
        outputZP     = (vx_float32)(output_attr->asymm.zero_point);
    }
#define _PACK_SELECT_KEY( IN_TYPE, OUT_TYPE )    \
        (IN_TYPE | ( OUT_TYPE << 16))

    pack_key = _PACK_SELECT_KEY(input_attr->dtype, output_attr->dtype );

    if (input_attr->dtype == F16
       || input_attr->dtype == I16
       || input_attr->dtype == BF16)
    {
        gpu_param.global_scale[0] = 8;
        gpu_param.global_scale[1] = 1;
        gpu_param.global_scale[2] = 1;
    }
    else
    {
        gpu_param.global_scale[0] = 16;
        gpu_param.global_scale[1] = 1;
        gpu_param.global_scale[2] = 1;
    }
    gpu_param.dim = out_shape->size < 3 ? 2 : 3;
    gpu_param.global_size[0]   = gpu_align_p2((out_shape->data[0] +  gpu_param.global_scale[0] - 1)
                                        /  gpu_param.global_scale[0], 4);
    gpu_param.global_size[1]   = (out_shape->data[1] +  gpu_param.global_scale[1] - 1)
                                        /  gpu_param.global_scale[1];
    gpu_param.global_size[2]   = out_shape->size > 2 ? out_shape->data[2] : 1;

    {
        gpu_dp_inst_t uniExtractInteger_2x8 = {{
            0x33333333, // TCfg
            0x11110000, // ASelt
            0x03020100, 0x03020100, // ABin
            0x00000000, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002400, // AccumType, ConstantType, and PostShift
            0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniExtractHalf8_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x06040200, 0x06040200, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00003c00, 0x00003c00, 0x00003c00,
            0x00003c00, 0x00003c00, 0x00003c00, 0x00003c00 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniDatatoFp32Part0_4x4 = {{
            0x01010101, // TCfg
            0x00000000, // ASelt
            0x00010000, 0x00030002, // ABin
            0x02020202, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniDatatoFp32Part1_4x4 = {{
            0x01010101, // TCfg
            0x00000000, // ASelt
            0x00050004, 0x00070006, // ABin
            0x02020202, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniConvBF16toF32_Part0_2x8 = {{
            0x11111111, // TCfg
            0x01010101, // ASelt
            0x01050004, 0x03070206, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniConvBF16toF32_Part1_2x8 = {{
            0x11111111, // TCfg
            0x01010101, // ASelt
            0x05050404, 0x07070606, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniExtractOddData_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x07050301, 0x07050301, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};

        switch (pack_key)
        {
            case _PACK_SELECT_KEY(BF16, BF16):
                {
                    status |= vsi_nn_kernel_gpu_add_param(node,
                        "uniConvBF16toF32_Part0_2x8", &uniConvBF16toF32_Part0_2x8);
                    status |= vsi_nn_kernel_gpu_add_param(node,
                        "uniConvBF16toF32_Part1_2x8", &uniConvBF16toF32_Part1_2x8);
                    status |= vsi_nn_kernel_gpu_add_param(node,
                        "uniExtractOddData_2x8", &uniExtractOddData_2x8);
                }
                break;
            default :
                {
                    if (F16 == output_attr->dtype)
                    {
                        status = vsi_nn_kernel_gpu_add_param(node, "uniExtract8Data_2x8", &uniExtractHalf8_2x8);
                    }
                    else
                    {
                        status = vsi_nn_kernel_gpu_add_param(node, "uniExtract8Data_2x8", &uniExtractInteger_2x8);
                    }
                    status |= vsi_nn_kernel_gpu_add_param(node, "inputScale", &inputScale);
                    status |= vsi_nn_kernel_gpu_add_param(node, "inputTail", &inputTail);
                    status |= vsi_nn_kernel_gpu_add_param(node, "outputScale", &outputScale);
                    status |= vsi_nn_kernel_gpu_add_param(node, "outputZP", &outputZP);
                    status |= vsi_nn_kernel_gpu_add_param(node, "uniDatatoFp32Part0_4x4", &uniDatatoFp32Part0_4x4);
                    status |= vsi_nn_kernel_gpu_add_param(node, "uniDatatoFp32Part1_4x4", &uniDatatoFp32Part1_4x4);
                }
                break;
        }

        status |= vsi_nn_kernel_gpu_add_param(node, "logE", &logE);
    }

    if( status == VSI_SUCCESS )
    {
        vsi_nn_kernel_gpu_config( node, &gpu_param );
    }

#undef _PACK_SELECT_KEY
final:
    if (input_attr)
    {
        vsi_nn_kernel_tensor_attr_release(&input_attr);
    }
    if (output_attr)
    {
        vsi_nn_kernel_tensor_attr_release(&output_attr);
    }

     return status;
} /* _swish_initializer() */

static vx_status VX_CALLBACK _hswish_initializer
    (
    vx_node              node,
    const vx_reference * param,
    uint32_t             param_size
    )
{
    vsi_status status = VX_SUCCESS;
    // Alignment with a power of two value.
    gpu_param_t gpu_param = {
        3,
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
        };

    vx_tensor     input            = (vx_tensor)param[0];
    vx_tensor     output           = (vx_tensor)param[1];
    int8_t        srcFixPointPos   = 0;
    int8_t        dstFixPointPos   = 0;
    vx_float32    inputTail        = 0;
    vx_float32    inputScale       = 1.0f;
    vx_float32    outputZP         = 0;
    vx_float32    outputScale      = 1.0f;
    vsi_nn_kernel_tensor_attr_t *input_attr = NULL, *output_attr = NULL;
    vsi_int_array_t             *out_shape  = NULL;
    uint32_t                     pack_key   = 0;

    input_attr  = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)input);
    if (NULL == input_attr)
    {
        VSILOGE("vsi_nn_kernel_tensor_attr_create failure! at line %d\n", __LINE__);
        status = VX_ERROR_NO_MEMORY;
        goto final;
    }
    output_attr = vsi_nn_kernel_tensor_attr_create( (vsi_nn_kernel_tensor_t)output);
    if (status != VX_SUCCESS)
    {
        VSILOGE("vsi_nn_kernel_tensor_attr_create failure! at line %d\n", __LINE__);
        status = VX_ERROR_NO_MEMORY;
        goto final;
    }

    out_shape = output_attr->shape;

    if (input_attr->quant == VSI_NN_KERNEL_QUANT_DFP)
    {
        srcFixPointPos = input_attr->dfp.fl;
        if (srcFixPointPos > 0)
        {
            inputScale = 1.0f / (vx_float32) (1 << srcFixPointPos);
        }
        else
        {
            inputScale = (vx_float32)(1 << -srcFixPointPos);
        }
    }
    else if (input_attr->quant == VSI_NN_KERNEL_QUANT_ASYMM || input_attr->quant == VSI_NN_KERNEL_QUANT_SYMM)
    {
        inputScale  = input_attr->asymm.scale;
        inputTail   = 0 - input_attr->asymm.zero_point * inputScale;
    }

    if (output_attr->quant == VSI_NN_KERNEL_QUANT_DFP)
    {
        dstFixPointPos = output_attr->dfp.fl;
        if (dstFixPointPos > 0)
        {
            outputScale = (vx_float32) (1 << dstFixPointPos);
        }
        else
        {
            outputScale = 1.0f / (vx_float32) (1 << -dstFixPointPos);
        }
    }
    else if (output_attr->quant == VSI_NN_KERNEL_QUANT_ASYMM || output_attr->quant == VSI_NN_KERNEL_QUANT_SYMM)
    {
        outputScale  = 1.0 / output_attr->asymm.scale;
        outputZP     = (vx_float32)(output_attr->asymm.zero_point);
    }
#define _PACK_SELECT_KEY( IN_TYPE, OUT_TYPE )    \
        (IN_TYPE | ( OUT_TYPE << 16))

    pack_key = _PACK_SELECT_KEY(input_attr->dtype, output_attr->dtype );

    if (input_attr->dtype == F16
       || input_attr->dtype == I16
       || input_attr->dtype == BF16)
    {
        gpu_param.global_scale[0] = 8;
        gpu_param.global_scale[1] = 1;
        gpu_param.global_scale[2] = 1;
    }
    else
    {
        gpu_param.global_scale[0] = 16;
        gpu_param.global_scale[1] = 1;
        gpu_param.global_scale[2] = 1;
    }
    gpu_param.dim = out_shape->size < 3 ? 2 : 3;
    gpu_param.global_size[0]   = gpu_align_p2((out_shape->data[0] +  gpu_param.global_scale[0] - 1)
                                        /  gpu_param.global_scale[0], 4);
    gpu_param.global_size[1]   = (out_shape->data[1] +  gpu_param.global_scale[1] - 1)
                                        /  gpu_param.global_scale[1];
    gpu_param.global_size[2]   = out_shape->size > 2 ? out_shape->data[2] : 1;

    {
        gpu_dp_inst_t uniExtractInteger_2x8 = {{
            0x33333333, // TCfg
            0x11110000, // ASelt
            0x03020100, 0x03020100, // ABin
            0x00000000, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00002400, // AccumType, ConstantType, and PostShift
            0x00000000, 0x00000000, 0x00000000, 0x00000000,
            0x00000000, 0x00000000, 0x00000000, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniExtractHalf8_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x06040200, 0x06040200, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00003c00, 0x00003c00, 0x00003c00,
            0x00003c00, 0x00003c00, 0x00003c00, 0x00003c00 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniDatatoFp32Part0_4x4 = {{
            0x01010101, // TCfg
            0x00000000, // ASelt
            0x00010000, 0x00030002, // ABin
            0x02020202, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniDatatoFp32Part1_4x4 = {{
            0x01010101, // TCfg
            0x00000000, // ASelt
            0x00050004, 0x00070006, // ABin
            0x02020202, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000100, // AccumType, ConstantType, and PostShift
            0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000, 0x00003c00, 0x00000000 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniConvBF16toF32_Part0_2x8 = {{
            0x11111111, // TCfg
            0x01010101, // ASelt
            0x01050004, 0x03070206, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniConvBF16toF32_Part1_2x8 = {{
            0x11111111, // TCfg
            0x01010101, // ASelt
            0x05050404, 0x07070606, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};
        gpu_dp_inst_t uniExtractOddData_2x8 = {{
            0x11111111, // TCfg
            0x11110000, // ASelt
            0x07050301, 0x07050301, // ABin
            0x22222222, // BSelt
            0x00000000, 0x00000000, // BBin
            0x00000600, // AccumType, ConstantType, and PostShift
            0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001 // Constant
        }, GPU_DP_TYPE_16};

        switch (pack_key)
        {
            case _PACK_SELECT_KEY(BF16, BF16):
                {
                    status |= vsi_nn_kernel_gpu_add_param( node,
                        "uniConvBF16toF32_Part0_2x8", &uniConvBF16toF32_Part0_2x8 );
                    status |= vsi_nn_kernel_gpu_add_param( node,
                        "uniConvBF16toF32_Part1_2x8", &uniConvBF16toF32_Part1_2x8);
                    status |= vsi_nn_kernel_gpu_add_param( node,
                        "uniExtractOddData_2x8", &uniExtractOddData_2x8);
                }
                break;
            default :
                {
                    if (F16 == output_attr->dtype)
                    {
                        status |= vsi_nn_kernel_gpu_add_param(node, "uniExtract8Data_2x8", &uniExtractHalf8_2x8);
                    }
                    else
                    {
                        status |= vsi_nn_kernel_gpu_add_param(node, "uniExtract8Data_2x8", &uniExtractInteger_2x8);
                    }
                    status |= vsi_nn_kernel_gpu_add_param(node, "inputScale", &inputScale);
                    status |= vsi_nn_kernel_gpu_add_param(node, "inputTail", &inputTail);
                    status |= vsi_nn_kernel_gpu_add_param(node, "outputScale", &outputScale);
                    status |= vsi_nn_kernel_gpu_add_param(node, "outputZP", &outputZP);
                    status |= vsi_nn_kernel_gpu_add_param(node, "uniDatatoFp32Part0_4x4", &uniDatatoFp32Part0_4x4);
                    status |= vsi_nn_kernel_gpu_add_param(node, "uniDatatoFp32Part1_4x4", &uniDatatoFp32Part1_4x4);
                }
                break;
        }
    }

    if( status == VSI_SUCCESS )
    {
        vsi_nn_kernel_gpu_config( node, &gpu_param );
    }

#undef _PACK_SELECT_KEY
final:
    if (input_attr)
    {
        vsi_nn_kernel_tensor_attr_release(&input_attr);
    }
    if (output_attr)
    {
        vsi_nn_kernel_tensor_attr_release(&output_attr);
    }
    return status;
} /* _hswish_initializer() */

/*
 * Query kernel
 */
static vsi_status _query_kernel
    (
    vsi_nn_kernel_t * kernel,
    vsi_nn_tensor_t * const * const inputs,
    vsi_nn_tensor_t * const * const outputs,
    vsi_bool image_2d,
    vsi_nn_swish_type swish_type
    /* Add extra params */
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_dtype_e in_dtype;
    vsi_nn_kernel_dtype_e out_dtype;
    const _kernel_map_type * kernel_map = _swish_kernel_map;
    size_t kernel_map_size              = _cnt_of_array( _swish_kernel_map );
    vx_param_description_t * param_def  = _swish_kernel_param_def;
    size_t param_def_size               = _cnt_of_array( _swish_kernel_param_def );
    vx_kernel_initialize_f  initializer = _swish_initializer;
    uint32_t key;
    int i;

    in_dtype  = vsi_nn_kernel_map_dtype( inputs[0]->attr.dtype.vx_type );
    out_dtype = vsi_nn_kernel_map_dtype( outputs[0]->attr.dtype.vx_type );

    key = SWISH_HASH_KEY(swish_type, in_dtype, out_dtype, image_2d);

    if (VSI_NN_HSWISH == swish_type)
    {
        initializer = _hswish_initializer;
    }
    else
    {
        initializer = _swish_initializer;
    }

    for( i = 0; i < kernel_map_size; i ++ )
    {
        if( kernel_map[i].key == key )
        {
            break;
        }
    }
    if( i < kernel_map_size )
    {
        snprintf( kernel->info.name, VX_MAX_KERNEL_NAME, "%s",  kernel_map[i].function_name );
        kernel->info.parameters  = param_def;
        kernel->info.numParams   = param_def_size;
        kernel->info.initialize  = initializer;
        // Register code source
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_CODE, 2,
                "vsi_nn_kernel_header",
                kernel_map[i].source_name );
        // Register binary source
        vsi_nn_kernel_add_source( kernel, VSI_NN_GPU_SOURCE_FMT_EXECUTABLE, 1,
                kernel_map[i].source_name );
        status = VSI_SUCCESS;
    }

    return status;
} /* _query_kernel() */


static vsi_nn_kernel_node_t _setup
    (
    vsi_nn_graph_t              * graph,
    vsi_nn_tensor_t            ** inputs,
    size_t                        input_num,
    vsi_nn_tensor_t            ** outputs,
    size_t                        output_num,
    const vsi_nn_kernel_param_t * params,
    vsi_nn_kernel_t             * kernel
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_kernel_node_param_t node_params[_SWISH_PARAM_NUM];
    int32_t  shapes[2][VSI_NN_MAX_DIM_NUM] = {{0}};
    uint32_t new_rank = 0;
    vsi_bool image_2d = FALSE;
    vsi_nn_kernel_node_t node = NULL;
    int32_t swish_type  = vsi_nn_kernel_param_get_int32( params, "type" );
    float   beta        = 1.0f;

    vsi_nn_OptimizedEltOPShape(inputs[0],  (uint32_t *)(shapes[0]), &new_rank);
    vsi_nn_OptimizedEltOPShape(outputs[0], (uint32_t *)(shapes[1]), &new_rank);

    if( !vsi_nn_kernel_gpu_check_shape( shapes[0], new_rank ) )
    {
        return NULL;
    }

    image_2d = (new_rank == 2);

     if (VSI_NN_HSWISH == (vsi_nn_swish_type)swish_type)
     {
        beta = 1.0f / 6.0f;
     }
     else
     {
        beta = vsi_nn_kernel_param_get_float32( params, "beta" );
     }

    status = _query_kernel( kernel, inputs, outputs, image_2d, (vsi_nn_swish_type)swish_type);
    if( VSI_SUCCESS == status)
    {
        node = vsi_nn_kernel_create_node( graph, kernel );
        if( node )
        {
            node_params[0] = vsi_nn_kernel_tensor_reshape( inputs[0]->t,  shapes[0], new_rank );
            node_params[1] = vsi_nn_kernel_tensor_reshape( outputs[0]->t, shapes[1], new_rank );
            node_params[2] = vsi_nn_kernel_scalar_create( graph, F32, &beta );
            /* Pass parameters to node. */
            status  = vsi_nn_kernel_node_pass_param( node, node_params, _SWISH_PARAM_NUM );
            VSI_ASSERT( status == VSI_SUCCESS );
            vsi_nn_kernel_tensor_release( &node_params[0] );
            vsi_nn_kernel_tensor_release( &node_params[1] );
            vsi_nn_kernel_scalar_release( &node_params[2] );

        }
    }

    return node;
} /* _setup() */

__END_DECLS

REGISTER_BACKEND_EVIS( swish, _setup )

