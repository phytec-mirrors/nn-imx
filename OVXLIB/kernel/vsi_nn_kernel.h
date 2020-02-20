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

#ifndef _VSI_NN_KERNEL_H
#define _VSI_NN_KERNEL_H

#include <stdint.h>
#include "vsi_nn_log.h"
#include "vsi_nn_ops.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_tensor.h"
#include "vsi_nn_daemon.h"
#include "vsi_nn_prv.h"
#include "utils/vsi_nn_util.h"
#include "utils/vsi_nn_shape_util.h"
#include "utils/vsi_nn_hashmap.h"
#include "utils/vsi_nn_math.h"
#include "kernel/vsi_nn_gpu.h"
#include "libnnext/vx_lib_nnext.h"

__BEGIN_DECLS

/** Kernel types */
typedef enum
{
    VSI_NN_KERNEL_TYPE_CPU = 0,
    VSI_NN_KERNEL_TYPE_EVIS,
    VSI_NN_KERNEL_TYPE_CL,
    VSI_NN_KERNEL_TYPE_VX,
    VSI_NN_KERNEL_TYPE_NUM,
    VSI_NN_KERNEL_TYPE_NONE = VSI_NN_KERNEL_TYPE_NUM
} vsi_nn_kernel_type_e;

/** Kernel internal data type */
typedef enum
{
    I8 = 0,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F16,
    F32,
    F64,
    BF16,
    BOOL8
} vsi_nn_kernel_dtype_e;

typedef enum
{
    VSI_NN_KERNEL_QUANT_NONE,
    VSI_NN_KERNEL_QUANT_DFP,
    VSI_NN_KERNEL_QUANT_ASYMM,
    VSI_NN_KERNEL_QUANT_ASYMM_PERCHANNEL,
    VSI_NN_KERNEL_QUANT_SYMM,
    VSI_NN_KERNEL_QUANT_SYMM_PERCHANNEL,
    VSI_NN_KERNEL_QUANT_TYPE_NUM
} vsi_nn_kernel_quant_type_e;

/** GPU source format */
typedef enum
{
    VSI_NN_GPU_SOURCE_FMT_CODE = 0,
    VSI_NN_GPU_SOURCE_FMT_EXECUTABLE = 1,
    VSI_NN_GPU_SOURCE_FMT_NUM
} vsi_nn_gpu_source_fmt_e;

typedef char * vsi_nn_kernel_source_t;
typedef uint32_t vsi_nn_kernel_unique_id_t;

typedef struct
{
   size_t num;
   vsi_nn_kernel_source_t * data;
   void * _reserve_mem;
} vsi_nn_kernel_source_info_t;

typedef struct
{
    vsi_nn_kernel_type_e      type;
    vsi_nn_kernel_unique_id_t unique_id;
    vx_kernel_description_t   info;
    struct
    {
        vsi_nn_kernel_source_info_t sources[VSI_NN_GPU_SOURCE_FMT_NUM];
        vsi_nn_gpu_source_fmt_e active_source_fmt;
    } gpu;
} vsi_nn_kernel_t;

typedef struct
{
    int32_t fl;
} vsi_nn_kernel_quant_dfp_t;

typedef struct
{
    float   scale;
    int32_t zero_point;
} vsi_nn_kernel_quant_asymm_t;

typedef struct
{
    vsi_float_array_t * scale;
    vsi_int_array_t   * zero_point;
    int32_t             channel_dim;
} vsi_nn_kernel_quant_asymm_perchannel_t;

typedef struct
{
    vsi_nn_kernel_dtype_e       dtype;
    vsi_int_array_t           * shape;
    vsi_nn_kernel_quant_type_e  quant;
    union
    {
        vsi_nn_kernel_quant_dfp_t dfp;
        vsi_nn_kernel_quant_asymm_t asymm;
        vsi_nn_kernel_quant_asymm_perchannel_t asymm_v;
    };
} vsi_nn_kernel_tensor_attr_t;

typedef void * vsi_nn_kernel_node_param_t;

typedef void * vsi_nn_kernel_tensor_t;

typedef void * vsi_nn_kernel_node_t;

typedef void * vsi_nn_kernel_scalar_t;

typedef vsi_nn_hashmap_t vsi_nn_kernel_param_t;

typedef vsi_nn_kernel_node_t (* vsi_nn_kernel_setup_func_t)
    (
    vsi_nn_graph_t *,
    vsi_nn_tensor_t **,
    size_t input_num,
    vsi_nn_tensor_t **,
    size_t output_num,
    const vsi_nn_kernel_param_t *,
    vsi_nn_kernel_t *
    );

typedef struct
{
    vsi_nn_kernel_unique_id_t  unique_id;
    vsi_nn_kernel_setup_func_t setup[VSI_NN_KERNEL_TYPE_NUM];
} vsi_nn_kernel_backend_t;

vsi_nn_kernel_param_t * vsi_nn_kernel_param_create();

void vsi_nn_kernel_param_release( vsi_nn_kernel_param_t ** params );

void vsi_nn_kernel_param_clear( vsi_nn_kernel_param_t * params );

vsi_bool vsi_nn_kernel_param_add_int32
    ( vsi_nn_kernel_param_t * params, const char * key, int32_t value);

int32_t vsi_nn_kernel_param_get_int32
    ( const vsi_nn_kernel_param_t * params, const char * key);

vsi_bool vsi_nn_kernel_param_add_int64
    ( vsi_nn_kernel_param_t * params, const char * key, int64_t value);

int64_t vsi_nn_kernel_param_get_int64
    ( const vsi_nn_kernel_param_t * params, const char * key);

vsi_bool vsi_nn_kernel_param_add_float32
    ( vsi_nn_kernel_param_t * params, const char * key, float value);

float vsi_nn_kernel_param_get_float32
    ( const vsi_nn_kernel_param_t * params, const char * key);

vsi_bool vsi_nn_kernel_param_add_str
    ( vsi_nn_kernel_param_t * params, const char * key, const char * str);

const char * vsi_nn_kernel_param_get_str
    ( const vsi_nn_kernel_param_t * params, const char * key);

vsi_bool vsi_nn_kernel_param_add_buffer
    ( vsi_nn_kernel_param_t * params, const char * key, void * buf, size_t size);

void * vsi_nn_kernel_param_get_buffer
    ( const vsi_nn_kernel_param_t * params, const char * key, size_t * size);

/** Kernel register */
#ifdef __cplusplus
    #define REGISTER_KERNEL_BACKEND(operation, kernel_type, func)   \
            _INITIALIZER(_register_kernel_##operation##_##kernel_type) \
            { \
                vsi_nn_kernel_backend_register( \
                        ""#operation, \
                        VSI_NN_KERNEL_TYPE_##kernel_type, func ); \
            }

#elif defined(_MSC_VER)
    #define REGISTER_KERNEL_BACKEND(operation, kernel_type, func)   \
            _INITIALIZER(_register_kernel_##operation##_##kernel_type) \
            { \
                vsi_nn_kernel_backend_register( \
                        ""#operation, \
                        VSI_NN_KERNEL_TYPE_##kernel_type, func ); \
            }
#elif defined(__linux__)
#if 1
    #define REGISTER_KERNEL_BACKEND(operation, kernel_type, func)   \
            _INITIALIZER(_register_kernel_##operation##_##kernel_type) \
            { \
                vsi_nn_kernel_backend_register( \
                        ""#operation, \
                        VSI_NN_KERNEL_TYPE_##kernel_type, func ); \
            }
#endif
#if 0
    typedef struct
    {
        const char* name;
        vsi_nn_op_t op;
        vsi_nn_kernel_type_e kernel_type;
        vsi_nn_kernel_setup_func_t func;
    } vsi_nn_kernel_section_meta_t;
    #define REGISTER_KERNEL_BACKEND(operation, kernel_type, func) \
        static vsi_nn_kernel_section_meta_t _kernel_meta = \
                {""#operation, VSI_NN_OP_##operation, VSI_NN_KERNEL_TYPE_##kernel_type, func}; \
        static vsi_nn_kernel_section_meta_t* _kernel_meta_ptr \
            __attribute__((section("kernel_meta_section"))) = &_kernel_meta;
#endif
#if 0
    #define REGISTER_KERNEL_BACKEND(operation, kernel_type, func)   \
                vsi_status func##_(vsi_nn_graph_t* graph, \
                        vsi_nn_tensor_t** inputs, size_t input_num, \
                        vsi_nn_tensor_t** outputs, size_t output_num) {\
                    return func(graph, inputs, input_num,  outputs, output_num); \
                }

    #define REGISTER_KERNEL_BACKEND_MANUALLY(operation, kernel_type, func) \
                extern vsi_status func##_(vsi_nn_graph_t*, \
                        vsi_nn_tensor_t** inputs, size_t input_num, \
                        vsi_nn_tensor_t** outputs, size_t output_num); \
                vsi_nn_kernel_backend_register( ""#operation, \
                        VSI_NN_KERNEL_TYPE_##kernel_type, func##_ );
#endif
#endif

#define REGISTER_BACKEND_CL(operation, func) \
    REGISTER_KERNEL_BACKEND(operation, CL, func)
#define REGISTER_BACKEND_EVIS(operation, func) \
    REGISTER_KERNEL_BACKEND(operation, EVIS, func)
#define REGISTER_BACKEND_CPU(operation, func) \
    REGISTER_KERNEL_BACKEND(operation, CPU, func)
#define REGISTER_BACKEND_OPENVX(operation, func) \
    REGISTER_KERNEL_BACKEND(operation, VX, func)

#define DEF_KERNEL_BASE_CALLBACK( NAME )  \
    static vsi_status NAME##_impl( vsi_nn_kernel_node_t node, \
            const vsi_nn_kernel_node_param_t * param, \
            size_t param_size ); \
    static vx_status VX_CALLBACK NAME( \
            vx_node node, const vx_reference * param,\
            vx_uint32 param_size) {\
                return (vx_status)NAME##_impl( \
                        (vsi_nn_kernel_node_t)node, \
                        (const vsi_nn_kernel_node_param_t *)param, \
                        (uint32_t)param_size \
                        ); \
            } \
    static vsi_status NAME##_impl

#define DEF_KERNEL_INITIALIZER( NAME )          DEF_KERNEL_BASE_CALLBACK( NAME )
#define DEF_KERNEL_EXECUTOR( NAME )             DEF_KERNEL_BASE_CALLBACK( NAME )
#define DEF_KERNEL_DEINITIALIZER( NAME )        DEF_KERNEL_BASE_CALLBACK( NAME )

void vsi_nn_kernel_backend_register
    (
    const char * kernel_name,
    vsi_nn_kernel_type_e kernel_type,
    vsi_nn_kernel_setup_func_t setup_func
    );

const vsi_nn_kernel_backend_t * vsi_nn_kernel_backend_get
    ( const char * );

vsi_status vsi_nn_kernel_backend_init( void );

void vsi_nn_kernel_backend_deinit( void );

vsi_nn_kernel_t * vsi_nn_kernel_create
    (
    vsi_nn_kernel_type_e type
    );

void vsi_nn_kernel_reset
    (
    vsi_nn_kernel_t * kernel,
    vsi_nn_kernel_type_e type
    );

void vsi_nn_kernel_release
    (
    vsi_nn_kernel_t ** kernel
    );

void vsi_nn_kernel_add_source
    (
    vsi_nn_kernel_t * kernel,
    vsi_nn_gpu_source_fmt_e fmt,
    size_t source_num,
    ...
    );

void vsi_nn_kernel_tensor_release
    (
    vsi_nn_kernel_tensor_t * tensor
    );

vsi_nn_kernel_tensor_t vsi_nn_kernel_tensor_reshape
    (
    vsi_nn_kernel_tensor_t tensor,
    int32_t * shape,
    uint32_t rank
    );

vsi_status vsi_nn_kernel_node_pass_param
    (
    vsi_nn_kernel_node_t node,
    vsi_nn_kernel_node_param_t * params,
    size_t num
    );

static inline void vsi_nn_kernel_node_release
    (
    vsi_nn_kernel_node_t * node
    )
{
    if( node && *node )
    {
        vxReleaseNode( (vx_node*)node );
    }
}

static inline void vsi_nn_kernel_node_pack_io
    (
    vsi_nn_kernel_node_param_t * params,
    size_t param_num,
    vsi_nn_tensor_t ** inputs,
    size_t input_num,
    vsi_nn_tensor_t ** outputs,
    size_t output_num
    )
{
    size_t i;
    size_t cnt;

    /* Set inputs */
    cnt = 0;
    for( i = 0; i < input_num && cnt < param_num; i ++, cnt ++ )
    {
        params[cnt] = (vsi_nn_kernel_node_param_t)(inputs[i]->t);
    }

    /* Set outputs */
    for( i = 0; i < output_num && cnt < param_num; i ++, cnt ++ )
    {
        params[cnt] = (vsi_nn_kernel_node_param_t)(outputs[i]->t);
    }
} /* vsi_nn_kernel_node_pack_io() */

/** Kernel selector */
vsi_nn_kernel_node_t vsi_nn_kernel_selector
    (
    vsi_nn_graph_t * graph,
    const char * kernel_name,
    vsi_nn_tensor_t ** inputs,
    size_t input_num,
    vsi_nn_tensor_t ** outputs,
    size_t output_num,
    const vsi_nn_kernel_param_t * params
    );

/** Map data type to gpu internal dtype. */
static inline vsi_nn_kernel_dtype_e vsi_nn_kernel_map_dtype
    (
    vsi_nn_type_e dtype
    )
{
    switch( dtype )
    {
    case VSI_NN_TYPE_INT8:
    case VSI_NN_TYPE_BOOL8:
        return I8;
    case VSI_NN_TYPE_INT16:
        return I16;
    case VSI_NN_TYPE_INT32:
        return I32;
    case VSI_NN_TYPE_INT64:
        return I64;
    case VSI_NN_TYPE_UINT8:
        return U8;
    case VSI_NN_TYPE_UINT16:
        return U16;
    case VSI_NN_TYPE_UINT32:
        return U32;
    case VSI_NN_TYPE_FLOAT16:
        return F16;
    case VSI_NN_TYPE_BFLOAT16:
        return BF16;
    case VSI_NN_TYPE_FLOAT32:
        return F32;
    default:
        VSILOGE("error data type %d", dtype);
        break;
    }
    return I8;
} /* vsi_nn_kernel_map_dtype() */

static inline size_t vsi_nn_kernel_dtype_get_bytes
    (
    vsi_nn_kernel_dtype_e dtype
    )
{
    switch( dtype )
    {
        case I8:
        case U8:
        case BOOL8:
            return sizeof(int8_t);
        case I16:
        case U16:
        case F16:
            return sizeof(int16_t);
        case I32:
        case U32:
        case F32:
            return sizeof(int32_t);
        case I64:
            return sizeof(int64_t);
        default:
            VSILOGE("Error data type %d", dtype);
            break;
    }
    return 0;
} /* vsi_nn_kernel_dtype_get_bytes() */

static inline vsi_nn_kernel_quant_type_e vsi_nn_kernel_map_quant_type
    ( vsi_nn_qnt_type_e quant_type )
{
    switch( quant_type )
    {
        case VSI_NN_QNT_TYPE_DFP:
            return VSI_NN_KERNEL_QUANT_DFP;
        case VSI_NN_QNT_TYPE_AFFINE_ASYMMETRIC:
            return VSI_NN_KERNEL_QUANT_ASYMM;
        case VSI_NN_QNT_TYPE_AFFINE_PERCHANNEL_SYMMETRIC:
            return VSI_NN_KERNEL_QUANT_SYMM_PERCHANNEL;
        default:
            break;
    }
    return VSI_NN_KERNEL_QUANT_NONE;
} /* vsi_nn_kernel_map_quant_type() */

vsi_nn_kernel_node_t  vsi_nn_kernel_create_node
    (
    vsi_nn_graph_t * graph,
    vsi_nn_kernel_t * kernel
    );

vsi_nn_kernel_scalar_t vsi_nn_kernel_scalar_create
    (
    vsi_nn_graph_t * graph,
    vsi_nn_kernel_dtype_e dtype,
    void * data
    );

static inline void vsi_nn_kernel_scalar_release
    ( vsi_nn_kernel_scalar_t * scalar )
{
    if( scalar && *scalar )
    {
        vxReleaseScalar( (vx_scalar*)scalar );
    }
} /* vsi_nn_kernel_scalar_relase() */

vsi_status vsi_nn_kernel_scalar_read_int8
    ( vsi_nn_kernel_scalar_t scalar, int8_t * out_data );

vsi_status vsi_nn_kernel_scalar_read_int32
    ( vsi_nn_kernel_scalar_t scalar, int32_t * out_data );

vsi_status vsi_nn_kernel_scalar_read_int64
    ( vsi_nn_kernel_scalar_t scalar, int64_t * out_data );

vsi_status vsi_nn_kernel_scalar_read_uint8
    ( vsi_nn_kernel_scalar_t scalar, uint8_t * out_data );

vsi_status vsi_nn_kernel_scalar_read_uint32
    ( vsi_nn_kernel_scalar_t scalar, uint32_t * out_data );

vsi_status vsi_nn_kernel_scalar_read_float32
    ( vsi_nn_kernel_scalar_t scalar, float * out_data );

vsi_status vsi_nn_kernel_scalar_read_float64
    ( vsi_nn_kernel_scalar_t scalar, double * out_data );

vsi_status vsi_nn_kernel_scalar_write_int8
    ( vsi_nn_kernel_scalar_t scalar, int8_t out_data );

vsi_status vsi_nn_kernel_scalar_write_int32
    ( vsi_nn_kernel_scalar_t scalar, int32_t out_data );

vsi_status vsi_nn_kernel_scalar_write_int64
    ( vsi_nn_kernel_scalar_t scalar, int64_t out_data );

vsi_status vsi_nn_kernel_scalar_write_uint8
    ( vsi_nn_kernel_scalar_t scalar, uint8_t out_data );

vsi_status vsi_nn_kernel_scalar_write_uint32
    ( vsi_nn_kernel_scalar_t scalar, uint32_t out_data );

vsi_status vsi_nn_kernel_scalar_write_float32
    ( vsi_nn_kernel_scalar_t scalar, float out_data );

vsi_status vsi_nn_kernel_scalar_write_float64
    ( vsi_nn_kernel_scalar_t scalar, double out_data );

vsi_status vsi_nn_kernel_scalar_get_dtype
    (
    vsi_nn_kernel_scalar_t scalar,
    vsi_nn_kernel_dtype_e * dtype
    );

vsi_status vsi_nn_kernel_register
    (
    vsi_nn_graph_t * graph,
    vsi_nn_kernel_t * kernel
    );

vsi_bool vsi_nn_kernel_gpu_check_shape
    ( const int32_t * shape, size_t rank );

vsi_status vsi_nn_kernel_gpu_add_param
    (
    vsi_nn_kernel_node_t node,
    const char * param_key,
    void * data
    );

vsi_status vsi_nn_kernel_gpu_config
    (
    vsi_nn_kernel_node_t node,
    const gpu_param_t * gpu_param
    );

vsi_nn_kernel_tensor_attr_t * vsi_nn_kernel_tensor_attr_create
    ( vsi_nn_kernel_tensor_t tensor );

void vsi_nn_kernel_tensor_attr_release
    ( vsi_nn_kernel_tensor_attr_t ** attr );

/*
 * Create a buffer with a copy of tensor data.
 * attr is optional
 */
void * vsi_nn_kernel_tensor_create_buffer
    (
    vsi_nn_kernel_tensor_t tensor,
    const vsi_nn_kernel_tensor_attr_t * attr,
    vsi_bool convert_to_float
    );

/*
 * Read tensor data to buffer.
 * attr is optional
 */
vsi_status vsi_nn_kernel_tensor_read
    (
    vsi_nn_kernel_tensor_t tensor,
    const vsi_nn_kernel_tensor_attr_t * attr,
    void * out_buffer,
    size_t out_buffer_size
    );

/*
 * Write float data to tensor.
 * attr is optional
 */
vsi_status vsi_nn_kernel_tensor_write_from_float
    (
    vsi_nn_kernel_tensor_t tensor,
    const vsi_nn_kernel_tensor_attr_t * attr,
    const float * float_buffer,
    size_t size
    );

/*
 * Write data to tensor.
 * attr is optional
 */
vsi_status vsi_nn_kernel_tensor_write
    (
    vsi_nn_kernel_tensor_t tensor,
    const vsi_nn_kernel_tensor_attr_t * attr,
    const void * buffer,
    size_t size
    );

static inline size_t vsi_nn_kernel_tensor_attr_get_size
    ( const vsi_nn_kernel_tensor_attr_t * attr )
{
    if( !attr )
    {
        return 0;
    }
    return vsi_nn_shape_get_size( attr->shape->data, attr->shape->size );
} /* vsi_nn_kernel_tensor_attr_get_size() */

static inline size_t vsi_nn_kernel_tensor_attr_get_bytes
    ( const vsi_nn_kernel_tensor_attr_t * attr )
{
    size_t size;
    size_t type_bytes;
    if( !attr )
    {
        return 0;
    }
    size = vsi_nn_kernel_tensor_attr_get_size( attr );
    type_bytes = vsi_nn_kernel_dtype_get_bytes( attr->dtype );
    return size * type_bytes;
} /* vsi_nn_kernel_tensor_attr_get_bytes() */

static inline void vsi_nn_kernel_tensor_attr_get_stride
    ( const vsi_nn_kernel_tensor_attr_t * attr, size_t * out_stride)
{
    if( !attr || !out_stride )
    {
        return;
    }
    vsi_nn_shape_get_stride( attr->shape->data, attr->shape->size, out_stride );
} /* vsi_nn_kernel_tensor_attr_get_size() */

static inline vsi_bool vsi_nn_kernel_tensor_attr_is_quantized
    ( const vsi_nn_kernel_tensor_attr_t * attr )
{
    return ( attr && attr->quant > VSI_NN_KERNEL_QUANT_NONE
            && attr->quant < VSI_NN_KERNEL_QUANT_TYPE_NUM );
} /* vsi_nn_kernel_tensor_attr_is_quantized() */

//TODO: Make vsi_nn_kernel_dtype_e to public and move dtype functions to vsi_nn_dtype.h
vsi_bool vsi_nn_dtype_convert_float_to_dtype
    (
    const float * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    void * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_asymm
    (
    const float * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    float scale, int32_t zero_point,
    void * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_dfp
    (
    const float * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    int32_t fl,
    void * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm
    (
    const float * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    float scale, int32_t zero_point,
    void * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_float_to_quantize_symm_perchannel
    (
    const float * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    const int32_t * shape, size_t rank,
    const float * scale, size_t scale_size,
    const int32_t * zero_point, size_t zero_point_size,
    int32_t channel_dim,
    void * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_dtype_to_float
    (
    const void * buffer,
    size_t size,
    vsi_nn_kernel_dtype_e dtype,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_asymm_to_float
    (
    const void * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_dfp_to_float
    (
    const void * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    int32_t fl,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm_to_float
    (
    const void * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    float scale, int32_t zero_point,
    float * out_buffer
    );

vsi_bool vsi_nn_dtype_convert_quantize_symm_perchannel_to_float
    (
    const void * buffer, size_t size,
    vsi_nn_kernel_dtype_e dtype,
    const int32_t * shape, size_t rank,
    const float * scale, size_t scale_size,
    const int32_t * zero_point, size_t zero_point_size,
    int32_t channel_dim,
    float * out_buffer
    );

__END_DECLS

#endif
