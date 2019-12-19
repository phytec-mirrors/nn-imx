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
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vsi_nn_context.h"
#include "vsi_nn_prv.h"
#include "vsi_nn_types.h"
#include "vsi_nn_graph.h"
#include "vsi_nn_log.h"
#include "vsi_nn_error.h"
#include "vsi_nn_context.h"
#include "kernel/vsi_nn_kernel.h"
#include "utils/vsi_nn_math.h"

#include "libnnext/vsi_nn_libnnext_resource.h"
#if VSI_USE_VXC_BINARY
/*this header can be only included once in all *.c files*/
#include "libnnext/vx_bin/vxc_binaries.h"
#endif

typedef struct
{
    size_t size;
    const void* data;
    void* reserve_mem;
} kernel_program_info_t;

static vsi_status _kernel_init_obj
    (
    vx_kernel_description_t* info,
    vx_kernel obj
    );

static vsi_status _cpu_register
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    );

static vsi_status _gpu_register
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    );

static vx_program _create_program_from_executable
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    );

static vx_program _create_program_from_code
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    );

static const void* _load_internal_executable
    (
    const char* source_name,
    size_t* size
    );

static char* _load_source_code_from_file
    (
    const char* source_name,
    size_t* size
    );

static vx_program _create_program
    (
    vx_context ctx,
    kernel_program_info_t *program_info,
    size_t size
    );

static void _kernel_clear_source
    ( vsi_nn_kernel_t * kernel );

static vsi_status VX_CALLBACK _kernel_validator
    (
    vx_node node,
    const vx_reference parameters[],
    uint32_t num,
    vx_meta_format metas[]
    )
{
    return VSI_SUCCESS;
} /* _kernel_validator() */

static vsi_status VX_CALLBACK _kernel_initializer
    (
    vx_node nodObj,
    const vx_reference *paramObj,
    uint32_t paraNum
    )
{
    return VSI_SUCCESS;
} /* _kernel_initializer() */

static vsi_status VX_CALLBACK _kernel_deinitializer
    (
    vx_node nodObj,
    const vx_reference *paraObj,
    uint32_t paraNum
    )
{
    return VSI_SUCCESS;
} /* _kernel_deinitializer() */

static void _kernel_clear_source
    ( vsi_nn_kernel_t * kernel )
{
    size_t i;
    if( !kernel )
    {
        return;
    }
    for( i = 0; i < VSI_NN_GPU_SOURCE_FMT_NUM; i ++ )
    {
        vsi_nn_kernel_source_info_t* source = &(kernel->gpu.sources[i]);
        if( source->data )
        {
            size_t j;
            for( j = 0; j < source->num; j ++ )
            {
                if( source->data[j] )
                {
                    free( source->data[j] );
                    source->data[j] = NULL;
                }
            }
            free( source->data );
            source->data = NULL;
        }
    }
} /* _kernel_clear_source() */

static vsi_status _cpu_register
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    )
{
    vsi_status status;
    vx_kernel_description_t* info;
    vx_kernel obj;

    status = VSI_FAILURE;
    info = &kernel->info;

    obj = vxAddUserKernel(
        graph->ctx->c,
        info->name,
        info->enumeration,
        info->function,
        info->numParams,
        info->validate,
        info->initialize,
        info->deinitialize
        );
    if( NULL != obj )
    {
        status = _kernel_init_obj( info, obj );
        //vxReleaseKernel( &obj );
    }
    else
    {
        VSILOGE( "Add kernel %s fail.", info->name );
    }
    return status;
} /* _cpu_register() */

static const void* _load_internal_executable
    (
    const char* source_name,
    size_t* size
    )
{
#if VSI_USE_VXC_BINARY
    int i;
    for( i = 0; i < vx_bin_resource_items_cnt; i++ )
    {
        if( strncmp( vx_bin_resource_items[i].name, source_name, VSI_NN_MAX_PATH ) == 0 )
        {
            *size = (size_t)vx_bin_resource_items[i].len;
            return vx_bin_resource_items[i].data;
        }
    }
#endif
    return NULL;
} /* _load_internal_executable() */

static char* _load_source_code_from_file
    (
    const char* source_name,
    size_t* size
    )
{
    char* source;
    FILE* fp;
    size_t total_bytes;
    size_t read_bytes;
    source = NULL;
    //TODO: Pack new name
    fp = fopen( source_name, "rb" );
    if( NULL == fp )
    {
        VSILOGE("Open program file %s fail.", source_name);
        *size = 0;
        goto final;
    }
    fseek( fp, 0, SEEK_END );
    total_bytes = ftell( fp );
    fseek( fp, 0, SEEK_SET );
    if( total_bytes == 0 )
    {
        VSILOGE("Program file %s is empty.", source_name);
        *size = 0;
        goto final;
    }
    source = (char*)malloc( total_bytes + 1 );
    if( source )
    {
        read_bytes = 0;
        while( total_bytes - read_bytes > 0 )
        {
            read_bytes += fread( &source[read_bytes], 1, total_bytes - read_bytes, fp );
        }
        source[read_bytes] = 0;
        *size = read_bytes;
    }
final:
    fclose( fp );
    return source;
} /* _load_source_code_from_file() */

static vx_program _create_program
    (
    vx_context ctx,
    kernel_program_info_t *program_info,
    size_t num
    )
{
    vx_char** sources;
    vx_size* source_sizes;
    size_t i;
    vsi_status status;
    vx_program program;
    program = NULL;

    sources = (vx_char**)malloc( sizeof(vx_char*) * num );
    source_sizes = (vx_size*)malloc( sizeof(vx_size) * num );

    for( i = 0; i < num; i ++ )
    {
        sources[i] = (vx_char*)program_info[i].data;
        source_sizes[i] = (vx_size)program_info[i].size;
    }
    program = vxCreateProgramWithSource( ctx, num, (const vx_char**)sources, source_sizes);
    status = vxGetStatus( (vx_reference)program );
    if(VSI_SUCCESS != status)
    {
        VSILOGE("Create program from source fail!");
    }
    if( sources )
    {
        free( sources );
    }
    if( source_sizes )
    {
        free( source_sizes );
    }
    return program;
} /* _create_program() */

static vx_program _create_program_from_code
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    )
{
    const vsi_nn_kernel_source_info_t* source_info;
    kernel_program_info_t* program_info;
    size_t i;
    vx_program program = NULL;
    source_info = &kernel->gpu.sources[VSI_NN_GPU_SOURCE_FMT_CODE];

    if( source_info->num == 0 )
    {
        VSILOGE("Not executable source found in kernel.");
        return NULL;
    }
    program_info = (kernel_program_info_t*)malloc(
            source_info->num * sizeof(kernel_program_info_t) );
    if( !program_info )
    {
        VSILOGE("Malloc program memory fail.");
        return NULL;
    }
    memset( program_info, 0, source_info->num * sizeof(kernel_program_info_t) );

    for( i = 0; i < source_info->num; i ++ )
    {
        program_info[i].data = (const void*)vsi_nn_resource_load_source_code(
                source_info->data[i], &program_info[i].size, kernel->type );
        if( !program_info[i].data )
        {
            program_info[i].reserve_mem = (void*)_load_source_code_from_file(
                    source_info->data[i], &program_info[i].size );
            program_info[i].data = (const void*)program_info[i].reserve_mem;
        }
    }
    program = _create_program( graph->ctx->c, program_info, source_info->num );
    if( program_info )
    {
        for( i = 0; i < source_info->num; i ++ )
        {
            if( program_info[i].reserve_mem )
            {
                free( program_info[i].reserve_mem );
            }
        }
        free( program_info );
    }
    return program;
} /* _create_program_from_code() */

static vx_program _create_program_from_executable
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    )
{
    const vsi_nn_kernel_source_info_t* source_info;
    kernel_program_info_t program_info;
    vx_program program = NULL;
    source_info = &kernel->gpu.sources[VSI_NN_GPU_SOURCE_FMT_EXECUTABLE];

    if( source_info->num == 0 )
    {
        VSILOGE("Not executable source found in kernel.");
        return NULL;
    }

    VSI_ASSERT( source_info->num == 1 );
    memset( &program_info, 0, sizeof( kernel_program_info_t ) );

    program_info.data = _load_internal_executable(
            source_info->data[0], &program_info.size);
    program = vxCreateProgramWithBinary( graph->ctx->c,
            (const vx_uint8 *)program_info.data, program_info.size );
    return program;
} /* _create_program_from_executable() */

static vsi_status _gpu_register
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    )
{
    vsi_status status;
    vx_kernel_description_t* info;
    vx_kernel obj;
    vsi_nn_context_t context;
    vx_program program = NULL;
#define MAX_BUILDPROGRAM_LEN 128
    char cmd[MAX_BUILDPROGRAM_LEN] = {0};

    memset( cmd, 0, sizeof(char) * MAX_BUILDPROGRAM_LEN );
    context = graph->ctx;

    status = VSI_FAILURE;
    info = &(kernel->info);

    switch( kernel->gpu.active_source_fmt )
    {
        case VSI_NN_GPU_SOURCE_FMT_CODE:
            program = _create_program_from_code( graph, kernel );
            break;
        case VSI_NN_GPU_SOURCE_FMT_EXECUTABLE:
            program = _create_program_from_executable( graph, kernel );
            break;
        default:
            VSILOGE("Unknown source format %d", kernel->gpu.active_source_fmt);
            break;
    }
    if( NULL == program )
    {
        return status;
    }

    if( context->config.evis.ver == VSI_NN_HW_EVIS_NONE )
    {
        // set default evis version is 2
        if( VSI_NN_KERNEL_TYPE_EVIS == kernel->type )
        {
            snprintf( cmd, MAX_BUILDPROGRAM_LEN, "-cl-viv-vx-extension -D VX_VERSION=2" );
        }
    }
    else
    {
        snprintf( cmd, MAX_BUILDPROGRAM_LEN, "-cl-viv-vx-extension -D VX_VERSION=%d",
                context->config.evis.ver );
    }

    status = vxBuildProgram( program, cmd );

    if( VSI_SUCCESS != status )
    {
        VSILOGE("Build program fail.");
        return status;
    }

    obj = vxAddKernelInProgram(
        program,
        info->name,
        info->enumeration,
        info->numParams,
        info->validate,
        info->initialize,
        info->deinitialize
        );

    if( obj )
    {
        status = _kernel_init_obj( info, obj );
        //vxReleaseKernel( &obj );
    }
    else
    {
        VSILOGE( "Add kernel %s fail.", info->name );
    }
    if( program )
    {
        vxReleaseProgram( &program );
    }
    return status;
} /* _gpu_register() */

static vsi_status _kernel_init_obj
    (
    vx_kernel_description_t* info,
    vx_kernel obj
    )
{
    vsi_status status;
    uint32_t i;

    status = VSI_SUCCESS;
    for( i = 0; i < info->numParams; i ++ )
    {
        status = vxAddParameterToKernel(
            obj,
            i,
            info->parameters[i].direction,
            info->parameters[i].data_type,
            info->parameters[i].state
            );
        if( VSI_SUCCESS != status )
        {
            VSILOGE( "Add parameter %d to kernel %s fail. with %d.",
                i, info->name, status );
            break;
        }
    }

    if( VSI_SUCCESS == status )
    {
        status = vxFinalizeKernel( obj );
    }

    if( VSI_SUCCESS != status )
    {
        VSILOGE( "Finalize kernel %s fail with %d.",
            info->name, status );
        status = vxRemoveKernel( obj );
        if( VSI_SUCCESS != status )
        {
            VSILOGE( "Remove kernel %s fail with %d.",
                info->name, status );
        }
    }
    return status;
} /* _kernel_init_obj() */

vsi_status vsi_nn_kernel_register
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    )
{
    vsi_status status;
    status = VSI_FAILURE;
    switch( kernel->type )
    {
    case VSI_NN_KERNEL_TYPE_CPU:
        status = _cpu_register( graph, kernel );
        break;
    case VSI_NN_KERNEL_TYPE_EVIS:
    case VSI_NN_KERNEL_TYPE_CL:
        status = _gpu_register( graph, kernel );
        break;
    case VSI_NN_KERNEL_TYPE_VX:
        VSILOGE("Openvx node no need to register.");
        break;
    default:
        VSILOGE("Unknown kernel %d.", kernel->type);
        break;
    }
    return status;
} /* vsi_nn_kernel_register() */

vsi_nn_kernel_node_t  vsi_nn_kernel_create_node
    (
    vsi_nn_graph_t* graph,
    vsi_nn_kernel_t* kernel
    )
{
    vsi_status status;
    vx_context ctx;
    vx_kernel obj;
    vx_node node;
    vx_kernel_description_t* info;

    info = &(kernel->info);
    // Validate kernel
    if( !info->initialize )
    {
        VSILOGE("Kernel %s initializer is NULL", info->name);
        return NULL;
    }
    if( !info->validate )
    {
        VSILOGE("Kernel %s validator is NULL", info->name);
        return NULL;
    }
    if( !info->deinitialize )
    {
        VSILOGE("Kernel %s deinitializer is NULL", info->name);
        return NULL;
    }
    if( info->enumeration == KERNEL_ID_PLACEHOLDER )
    {
        //VSILOGD("Kernel id: %#x, %#x", kernel->unique_id, info->enumeration);
        info->enumeration = (vx_enum)kernel->unique_id;
    }
    if( info->enumeration > KERNEL_ID_OVXLIB_RESERVED )
    {
        VSILOGE("Kernel id is invalid %#x(max: %#x)",
                info->enumeration, KERNEL_ID_OVXLIB_RESERVED);
        return NULL;
    }

    ctx = vxGetContext( (vx_reference)graph->g );

    obj = vxGetKernelByName( ctx, info->name );
    fprintf(stderr, "\n"); // TODO: This is a hack for driver msg
    status = vxGetStatus( (vx_reference)obj );
    if (VSI_SUCCESS != status)
    {
        /* Register kernel */
        status = vsi_nn_kernel_register( graph, kernel );
        if( VSI_SUCCESS != status )
        {
            VSILOGE( "Register client kernel %s fail with %d.",
                info->name, status );
            return NULL;
        }
        else
        {
            VSILOGD( "Register client kernel %s successfully.",
                info->name );
        }

        /* Load kernel */
        obj = vxGetKernelByName( ctx, info->name );
        status = vxGetStatus( (vx_reference)obj );
    }
    if( VSI_SUCCESS != status )
    {
        VSILOGE( "Load client kernel %s fail with %d.",
            info->name, status );
        return NULL;
    }
    node = vxCreateGenericNode( graph->g, obj );
    vxReleaseKernel( &obj );
    status = vxGetStatus( (vx_reference)node );
    if( VSI_SUCCESS != status )
    {
        VSILOGE( "Load client node from kernel %s fail with %d.",
            info->name, status );
        return NULL;
    }
    if( node )
    {
        // Set default border mode.
        vx_border_t border;
        border.mode = VX_BORDER_REPLICATE;
        border.constant_value.U32 = 0;
        status |= vxSetNodeAttribute( node, VX_NODE_BORDER, &border, sizeof(border) );
    }
    return (vsi_nn_kernel_node_t)node;
} /* vsi_nn_kernel_create_node() */

vsi_status vsi_nn_kernel_node_pass_param
    (
    vsi_nn_kernel_node_t node,
    vsi_nn_kernel_node_param_t * params,
    size_t num
    )
{
    vsi_status status;
    size_t i;

    status = VSI_FAILURE;
    for( i = 0; i < num; i++ )
    {
        status = vxSetParameterByIndex( (vx_node)node, i, (vx_reference)params[i] );
        if( VSI_SUCCESS != status )
        {
            VSILOGE( "Set %d parameter fail.", i );
            break;
        }
    }
    return status;
} /* vsi_nn_kernel_node_pass_param() */

vsi_nn_kernel_tensor_t vsi_nn_kernel_tensor_reshape
    (
    vsi_nn_kernel_tensor_t tensor,
    int32_t* shape,
    uint32_t rank
    )
{
    return (vsi_nn_kernel_tensor_t)vxReshapeTensor((vx_tensor)tensor, shape, rank);
} /* vsi_nn_kernel_tensor_reshape() */

void vsi_nn_kernel_tensor_release
    (
    vsi_nn_kernel_tensor_t* tensor
    )
{
    vxReleaseTensor( (vx_tensor*)tensor );
    *tensor = NULL;
} /* vsi_nn_kernel_tensor_release() */

void vsi_nn_kernel_add_source
    (
    vsi_nn_kernel_t* kernel,
    vsi_nn_gpu_source_fmt_e fmt,
    size_t source_num,
    ...
    )
{
    va_list arg;
    size_t i;
    vsi_nn_kernel_source_info_t* source;
    if( source_num == 0 )
    {
        return;
    }
    if( fmt >= VSI_NN_GPU_SOURCE_FMT_NUM )
    {
        VSILOGE("Unknown source type %d", fmt);
        return;
    }
    if( kernel->gpu.sources[fmt].data )
    {
        VSILOGE("Kernel source %d has been attached!", fmt);
        return;
    }
    source = &(kernel->gpu.sources[fmt]);
    va_start( arg, source_num );
    if( source_num > 0 )
    {
        const size_t mem_size = sizeof(vsi_nn_kernel_source_t) * source_num;
        source->data = (vsi_nn_kernel_source_t*)malloc( mem_size );
        if( !source->data )
        {
            VSILOGE("Out of memory, create kernel source fail.");
            return;
        }
        memset( source->data, 0, mem_size );
    }
    for( i = 0; i < source_num; i ++ )
    {
        vsi_nn_kernel_source_t src = va_arg( arg, vsi_nn_kernel_source_t );
        size_t size = strlen( src );
        source->data[i] = (vsi_nn_kernel_source_t)malloc( size * sizeof(char) + 1 );
        if( source->data[i] )
        {
            memcpy( source->data[i], src, size );
            source->data[i][size] = 0;
        }
        else
        {
            VSILOGE("Malloc source memory fail.");
            return;
        }
    }
    source->num = source_num;
    va_end(arg);
} /* vsi_nn_kernel_add_source() */

void vsi_nn_kernel_release
    (
    vsi_nn_kernel_t ** kernel
    )
{
    if( kernel && *kernel )
    {
        _kernel_clear_source( *kernel );
        free( *kernel );
        *kernel = NULL;
    }
} /* vsi_nn_kernel_release() */

vsi_nn_kernel_t* vsi_nn_kernel_create
    (
    vsi_nn_kernel_type_e type
    )
{
    vsi_nn_kernel_t* kernel = (vsi_nn_kernel_t*)malloc( sizeof(vsi_nn_kernel_t) );
    if( !kernel )
    {
        VSILOGE("Out of memory, create kernel fail.");
        return NULL;
    }
    /*
     * !!!WARNING!!!
     * Here must reset memory to 0, or vsi_nn_kernel_reset will crash.
     */
    memset( kernel, 0, sizeof(vsi_nn_kernel_t) );
    vsi_nn_kernel_reset( kernel, type );
    return kernel;
} /* vsi_nn_kernel_create() */

void vsi_nn_kernel_reset
    (
    vsi_nn_kernel_t * kernel,
    vsi_nn_kernel_type_e type
    )
{
    if( kernel )
    {
        _kernel_clear_source( kernel );
        memset( kernel, 0, sizeof(vsi_nn_kernel_t) );
        kernel->type = type;
        // TODO: Choose binary
#if VSI_USE_VXC_BINARY
        kernel->gpu.active_source_fmt = VSI_NN_GPU_SOURCE_FMT_EXECUTABLE;
#else
        kernel->gpu.active_source_fmt = VSI_NN_GPU_SOURCE_FMT_CODE;
#endif
        // Set default functions.
        kernel->info.enumeration = KERNEL_ID_PLACEHOLDER;
        kernel->info.initialize = _kernel_initializer;
        kernel->info.validate = _kernel_validator;
        kernel->info.deinitialize = _kernel_deinitializer;
    }
} /* vsi_nn_kernel_reset() */

vsi_nn_kernel_node_t vsi_nn_kernel_selector
    (
    vsi_nn_graph_t* graph,
    const char* kernel_name,
    vsi_nn_tensor_t** inputs,
    size_t input_num,
    vsi_nn_tensor_t** outputs,
    size_t output_num,
    const vsi_nn_kernel_param_t* params
    )
{
    vsi_nn_kernel_node_t node = NULL;
    vsi_nn_kernel_t * kernel;
    const vsi_nn_kernel_backend_t* backend;
    if( !kernel_name )
    {
        VSI_ASSERT( FALSE );
        return NULL;
    }
    if( !graph )
    {
        VSI_ASSERT( FALSE );
        return NULL;
    }

    backend = vsi_nn_kernel_backend_get( kernel_name );
    if( !backend )
    {
        VSILOGW("Not found kernel \"%s\"", kernel_name);
        return NULL;
    }
    kernel = vsi_nn_kernel_create( VSI_NN_KERNEL_TYPE_NONE );
    if( !kernel )
    {
        return NULL;
    }
    /**
     * All kernels for one operation will share the same id.
     */

    #define TRY_KERNEL_FUNC( kernel_type )  \
        do { \
            vsi_nn_kernel_setup_func_t kernel_func = backend->setup[kernel_type]; \
            if( NULL == kernel_func ) \
            { \
                break; \
            }\
            vsi_nn_kernel_reset( kernel, kernel_type ); \
            kernel->unique_id = KERNEL_ID_OVXLIB_START + backend->unique_id; \
            node = kernel_func( graph, inputs, input_num, \
                    outputs, output_num, params, kernel ); \
            if( node ) { \
                goto final; \
            } \
        } while(0)
    // TODO: This is a simple selector.
    //if support evis
    if( graph->ctx->config.evis.ver != VSI_NN_HW_EVIS_NONE )
    {
        TRY_KERNEL_FUNC( VSI_NN_KERNEL_TYPE_EVIS );
    }
    //if support cl
    TRY_KERNEL_FUNC( VSI_NN_KERNEL_TYPE_CL );
    //if use vx
    TRY_KERNEL_FUNC( VSI_NN_KERNEL_TYPE_VX );
    //if use cpu
    TRY_KERNEL_FUNC( VSI_NN_KERNEL_TYPE_CPU );
    #undef TRY_KERNEL_FUNC
final:
    if( !node )
    {
        VSILOGW("No valid kernel for %s", kernel_name);
    }
    vsi_nn_kernel_release( &kernel );

    return node;
} /* vsi_nn_kernel_selector() */

vsi_bool vsi_nn_kernel_gpu_check_shape
    ( const int32_t * shape, size_t rank )
{
    size_t i;
    vsi_bool ret = TRUE;
    const size_t channel_dim = 2;
    for( i = 0; i < vsi_nn_min(rank, channel_dim); i++ )
    {
        if( shape[i] == 0
         || shape[i] >= GPU_TENSOR_MAX_WIDTH )
        {
            ret = FALSE;
            break;
        }
    }
    return ret;
} /* vsi_nn_kernel_gpu_check_shape() */

vsi_nn_kernel_scalar_t vsi_nn_kernel_scalar_create
    (
    vsi_nn_graph_t * graph,
    vsi_nn_kernel_dtype_e dtype,
    void * data
    )
{
    vx_enum vxtype = VX_TYPE_FLOAT32;
    if( !graph || !data )
    {
        return NULL;
    }
    switch( dtype )
    {
        case I8:
            vxtype = VX_TYPE_INT8;
            break;
        case I16:
            vxtype = VX_TYPE_INT16;
            break;
        case I32:
            vxtype = VX_TYPE_INT32;
            break;
        case I64:
            vxtype = VX_TYPE_INT64;
            break;
        case U8:
            vxtype = VX_TYPE_UINT8;
            break;
        case U16:
            vxtype = VX_TYPE_UINT16;
            break;
        case U32:
            vxtype = VX_TYPE_UINT32;
            break;
        case U64:
            vxtype = VX_TYPE_UINT64;
            break;
        case F16:
            vxtype = VX_TYPE_FLOAT16;
            break;
        case F32:
            vxtype = VX_TYPE_FLOAT32;
            break;
        case BF16:
        case BOOL8:
        default:
            VSILOGW("Unsupport dtype %d", dtype);
            return NULL;
    }
    return vxCreateScalar( graph->ctx->c, vxtype, data );
}  /* vsi_nn_kernel_scalar_create() */

vsi_status vsi_nn_kernel_gpu_add_param
    (
    vsi_nn_kernel_node_t node,
    const char * param_key,
    void * data
    )
{
    return vxSetNodeUniform( (vx_node)node, param_key, 1, data );
} /* vsi_nn_kernel_gpu_add_param() */

vsi_status vsi_nn_kernel_gpu_config
    (
    vsi_nn_kernel_node_t node,
    const gpu_param_t * gpu_param
    )
{
    vsi_status status;
    vx_kernel_execution_parameters_t param;
    param.workDim = gpu_param->dim;
    memcpy( param.globalWorkOffset, gpu_param->global_offset,
            sizeof(size_t) * GPU_MAX_DIMENSION_SIZE );
    memcpy( param.globalWorkScale, gpu_param->global_scale,
            sizeof(size_t) * GPU_MAX_DIMENSION_SIZE );
    memcpy( param.localWorkSize, gpu_param->local_size,
            sizeof(size_t) * GPU_MAX_DIMENSION_SIZE );
    memcpy( param.globalWorkSize, gpu_param->global_size,
            sizeof(size_t) * GPU_MAX_DIMENSION_SIZE );
    status = vxSetNodeAttribute( (vx_node)node,
            VX_NODE_ATTRIBUTE_KERNEL_EXECUTION_PARAMETERS,
            &param, sizeof(vx_kernel_execution_parameters_t) );
    return status;
} /* vsi_nn_kernel_gpu_config() */

vsi_nn_kernel_tensor_attr_t * vsi_nn_kernel_tensor_attr_create
    ( vsi_nn_kernel_tensor_t tensor )
{
    vsi_nn_kernel_tensor_attr_t * attr;
    vsi_status status;
    uint32_t dim_num;
    vsi_nn_type_e dtype = VSI_NN_TYPE_FLOAT16;
    vsi_nn_qnt_type_e quant_type = VSI_NN_QNT_TYPE_NONE;
    attr = (vsi_nn_kernel_tensor_attr_t *)malloc(
            sizeof(vsi_nn_kernel_tensor_attr_t) );
    if( !attr )
    {
        VSILOGE("Out of memory, create tensor attr fail!");
        return NULL;
    }
    memset( attr, 0, sizeof(vsi_nn_kernel_tensor_attr_t) );

    status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_NUM_OF_DIMS,
        &dim_num, sizeof(uint32_t));
    CHECK_STATUS( status );
    if( status == VSI_SUCCESS )
    {
        vsi_int_array_t * shape = vsi_int_array_create( dim_num );
        if( shape )
        {
            status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_DIMS,
                shape->data, sizeof(int32_t) * dim_num);
            attr->shape = shape;
            CHECK_STATUS( status );
        }
    }
    status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_DATA_TYPE,
        &dtype, sizeof(vsi_enum));
    CHECK_STATUS( status );
    attr->dtype = vsi_nn_kernel_map_dtype( dtype );

    status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_QUANT_FORMAT,
        &quant_type, sizeof(uint32_t));
    CHECK_STATUS( status );
    attr->quant = vsi_nn_kernel_map_quant_type( quant_type );

    switch( attr->quant )
    {
    case VSI_NN_KERNEL_QUANT_DFP:
        {
        int8_t fl = 0;
        status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_FIXED_POINT_POS,
            &fl, sizeof(int8_t));
        CHECK_STATUS( status );
        attr->dfp.fl = (int32_t)fl;
        }
        break;
    case VSI_NN_KERNEL_QUANT_ASYMM:
        {
        status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_ZERO_POINT,
            &(attr->asymm.zero_point), sizeof(int32_t));
        CHECK_STATUS( status );
        status = vxQueryTensor( (vx_tensor)tensor, VX_TENSOR_SCALE,
            &(attr->asymm.scale), sizeof(float));
        CHECK_STATUS( status );
        // Reset scale to 1
        if( (attr->asymm.scale - 0.f) < 1e-5 )
            {
            attr->asymm.scale = 1.0f;
            attr->asymm.zero_point = 0;
            }
        }
        break;
    default:
        break;
    }
    return attr;
} /* vsi_nn_kernel_tensor_attr_create() */

void vsi_nn_kernel_tensor_attr_release
    ( vsi_nn_kernel_tensor_attr_t ** p_attr )
{
    if( p_attr && *p_attr )
    {
        vsi_nn_kernel_tensor_attr_t * attr = *p_attr;
        vsi_int_array_release( &attr->shape );
        if( attr->quant == VSI_NN_KERNEL_QUANT_ASYMM_PERCHANNEL )
        {
            vsi_float_array_release( &attr->asymm_v.scale );
            vsi_int_array_release( &attr->asymm_v.zero_point );
        }
        else if( attr->quant == VSI_NN_KERNEL_QUANT_SYMM_PERCHANNEL )
        {
            //TODO:
        }
        free( attr );
        *p_attr = NULL;
    }
} /* vsi_nn_kernel_tensor_attr_release() */
