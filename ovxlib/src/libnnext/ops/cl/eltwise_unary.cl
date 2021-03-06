
float4 eltwise_unary_sin(float4 x)
{
    return sin(x);
}

#define logE    (1.44269502f)
float4 eltwise_unary_exp(float4 x)
{
    x *= logE;
    x = exp2(x);
    return x;
}

#define rlogE    (0.693147182f)
float4 eltwise_unary_log(float4 x)
{
    x = log2(x);
    return x * rlogE;
}

float4 eltwise_unary_elu(float4 val)
{
    float4 x = val * logE;
    x = exp2(x) - 1;

    return val < 0 ? x : val;
}

float4 eltwise_unary_neg(float4 x)
{
    return x * -1;
}


#define ELTWISE_UNARY_F32(func_name) \
__kernel void func_name##_F32toF32 \
    ( \
    __read_only  image2d_array_t input, \
    __write_only image2d_array_t output, \
                 float           inputScale, \
                 float           inputTail, \
                 float           outputScale, \
                 float           outputZP \
    ) \
{ \
    int4 coord =  (int4)(get_global_id(0), get_global_id(1), get_global_id(2), 0); \
 \
    float4 src = read_imagef(input, coord); \
 \
    float4 dst = eltwise_unary_##func_name(src); \
 \
    write_imagef(output, coord, dst); \
}
ELTWISE_UNARY_F32(sin)
ELTWISE_UNARY_F32(exp)
ELTWISE_UNARY_F32(log)
ELTWISE_UNARY_F32(elu)
ELTWISE_UNARY_F32(neg)

#define ELTWISE_UNARY_F32_2D(func_name) \
__kernel void func_name##_F32toF32_2D \
    ( \
    __read_only  image2d_t input, \
    __write_only image2d_t output, \
                 float     inputScale, \
                 float     inputTail, \
                 float     outputScale, \
                 float     outputZP \
    ) \
{ \
    int2 coord =  (int2)(get_global_id(0), get_global_id(1)); \
 \
    float4 src = read_imagef(input, coord); \
 \
    float4 dst = eltwise_unary_##func_name(src); \
 \
    write_imagef(output, coord, dst); \
}
ELTWISE_UNARY_F32_2D(sin)
ELTWISE_UNARY_F32_2D(exp)
ELTWISE_UNARY_F32_2D(log)
ELTWISE_UNARY_F32_2D(elu)
ELTWISE_UNARY_F32_2D(neg)

#define ELTWISE_UNARY_U8(func_name) \
__kernel void func_name##_U8toU8 \
    ( \
    __read_only  image2d_array_t input, \
    __write_only image2d_array_t output, \
                 float           inputScale, \
                 float           inputTail, \
                 float           outputScale, \
                 float           outputZP \
    ) \
{ \
    int4 coord =  (int4)(get_global_id(0), get_global_id(1), get_global_id(2), 0); \
 \
    uint4 src = read_imageui(input, coord); \
    float4 data = convert_float4(src) * inputScale - inputTail; \
 \
    data = eltwise_unary_##func_name(data); \
    uint4 dst = convert_uint4(data * outputScale + outputZP); \
 \
    write_imageui(output, coord, dst); \
}
ELTWISE_UNARY_U8(sin)
ELTWISE_UNARY_U8(exp)
ELTWISE_UNARY_U8(log)
ELTWISE_UNARY_U8(elu)
ELTWISE_UNARY_U8(neg)

#define ELTWISE_UNARY_U8_2D(func_name) \
__kernel void func_name##_U8toU8_2D \
    ( \
    __read_only  image2d_t input, \
    __write_only image2d_t output, \
                 float     inputScale, \
                 float     inputTail, \
                 float     outputScale, \
                 float     outputZP \
    ) \
{ \
    int2 coord =  (int2)(get_global_id(0), get_global_id(1)); \
 \
    uint4 src = read_imageui(input, coord); \
    float4 data = convert_float4(src) * inputScale - inputTail; \
 \
    data = eltwise_unary_##func_name(data); \
    uint4 dst = convert_uint4(data * outputScale + outputZP); \
 \
    write_imageui(output, coord, dst); \
}
ELTWISE_UNARY_U8_2D(sin)
ELTWISE_UNARY_U8_2D(exp)
ELTWISE_UNARY_U8_2D(log)
ELTWISE_UNARY_U8_2D(elu)
ELTWISE_UNARY_U8_2D(neg)


__kernel void neg_I32toI32
    (
    __read_only  image2d_array_t input,
    __write_only image2d_array_t output,
                 float           inputScale,
                 float           inputTail,
                 float           outputScale,
                 float           outputZP
    )
{
    int4 coord = (int4)(get_global_id(0), get_global_id(1), get_global_id(2), 0);
    int4 src = read_imagei(input, coord);

    int4 dst = -src;

    write_imagei(output, coord, dst);
}

__kernel void neg_I32toI32_2D
    (
    __read_only  image2d_t input,
    __write_only image2d_t output,
                 float     inputScale,
                 float     inputTail,
                 float     outputScale,
                 float     outputZP
    )
{
    int2 coord =  (int2)(get_global_id(0), get_global_id(1));
    int4 src = read_imagei(input, coord);

    int4 dst = -src;

    write_imagei(output, coord, dst);
}
