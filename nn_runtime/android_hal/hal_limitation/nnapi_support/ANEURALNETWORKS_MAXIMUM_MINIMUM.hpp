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

// Note: Compatible with ADD, SUB, MUL, DIV

#ifndef __ANEURALNETWORKS_MAXIMUM_MINIMUM_HPP__
#define __ANEURALNETWORKS_MAXIMUM_MINIMUM_HPP__

#include "hal_limitation/support_macros.hpp"


// Input Spec
#define OP_SPEC_NAME MaximumMinimumInput
OP_SPEC_BEGIN()
#define ARG_NAMES         \
    (input0,     \
    input1)
#define ARGC BOOST_PP_TUPLE_SIZE(ARG_NAMES)

#define BOOST_PP_LOCAL_MACRO(n) OP_SPEC_ARG(BOOST_PP_TUPLE_ELEM(ARGC, n, ARG_NAMES))
#define BOOST_PP_LOCAL_LIMITS (0, ARGC)
#include BOOST_PP_LOCAL_ITERATE()
OP_SPEC_END()

// order of argument is important
MAKE_SPEC(maximum_minimum)
    .input0_(nnrt::OperandType::TENSOR_FLOAT32)
    .input1_(nnrt::OperandType::TENSOR_FLOAT32));

    OVERRIDE_SPEC(maximum_minimum, float16)
    .input0_(nnrt::OperandType::TENSOR_FLOAT16)
    .input1_(nnrt::OperandType::TENSOR_FLOAT16));

    // OVERRIDE_SPEC(maximum_minimum, int32)
    // .input0_(nnrt::OperandType::TENSOR_INT32)
    // .input1_(nnrt::OperandType::TENSOR_INT32));

    OVERRIDE_SPEC(maximum_minimum, asymm_u8)
    .input0_(nnrt::OperandType::TENSOR_QUANT8_ASYMM)
    .input1_(nnrt::OperandType::TENSOR_QUANT8_ASYMM));

#undef ARG_NAMES
#undef ARGC
#undef OP_SPEC_NAME

// Output Spec
#define OP_SPEC_NAME MaximumMinimumOutput
OP_SPEC_BEGIN()
#define ARG_NAMES         \
    (input0,     \
    output)
#define ARGC BOOST_PP_TUPLE_SIZE(ARG_NAMES)

#define BOOST_PP_LOCAL_MACRO(n) OP_SPEC_ARG(BOOST_PP_TUPLE_ELEM(ARGC, n, ARG_NAMES))
#define BOOST_PP_LOCAL_LIMITS (0, ARGC)
#include BOOST_PP_LOCAL_ITERATE()
OP_SPEC_END()

// order of argument is important
MAKE_SPEC(output)
    .input0_(nnrt::OperandType::TENSOR_FLOAT32)
    .output_(nnrt::OperandType::TENSOR_FLOAT32));

    OVERRIDE_SPEC(output, 0)
    .input0_(nnrt::OperandType::TENSOR_FLOAT16)
    .output_(nnrt::OperandType::TENSOR_FLOAT16));

    OVERRIDE_SPEC(output, 1)
    .input0_(nnrt::OperandType::TENSOR_QUANT8_ASYMM)
    .output_(nnrt::OperandType::TENSOR_QUANT8_ASYMM));

    // OVERRIDE_SPEC(output, 2)
    // .input0_(nnrt::OperandType::TENSOR_INT32)
    // .output_(nnrt::OperandType::TENSOR_INT32));

#undef ARG_NAMES
#undef ARGC
#undef OP_SPEC_NAME

#endif