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
#include "vsi_nn_pub.h"
#include "prepared_model.hpp"
#include "error.hpp"
#include "ovxlib_delegate.hpp"
#include "model_transform/transformations.hpp"

namespace nnrt
{
PreparedModel::PreparedModel(Model* model,
        SharedContextPtr context, Interpreter* interpreter)
    : model_(model)
    , interpreter_(interpreter)
    , context_(context)
{}

PreparedModel::~PreparedModel()
{
    vsi_nn_ReleaseGraph(&graph_);
}

int PreparedModel::prepare()
{
    if (graph_) {
        return NNA_ERROR_CODE(NO_ERROR);
    }
    NNRT_LOGD_PRINT("Prepare model ...");
    // Compile
    int err = NNA_ERROR_CODE(NO_ERROR);
    if (!model_->isCompiled()) {
        TransformationSet transformations;
        bool modified = false;
        // NOTE: The transformation order is important.
        model_->echo();
        transformations.add(new AlignBroadcastOp());
        transformations.add(new LayoutInference());
        transformations.add(new OptimizePermute());
        transformations.add(new ValidateQuantizedGraph());

        // relaxed mode
        if (model_->isRelaxed())
            transformations.add(new Fp32ToFp16());

        if(interpreter_) {
            interpreter_->run(model_, &modified);
        }

        err = transformations.once(model_);
        model_->freezeCompile();
        if (err != NNA_ERROR_CODE(NO_ERROR)) {
            return err;
        }
        model_->echo();
    }
    // Build ovxlib graph
    OvxlibDelegate delegate;
    err = delegate.process(model_, context_.get());
    tensor_mapping_ = delegate.getTensorMapping();
    graph_ = delegate.throwGraph();
    if (err != NNA_ERROR_CODE(NO_ERROR)) {
        NNRT_LOGE_PRINT("Prepare graph fail.");
        vsi_nn_ReleaseGraph(&graph_);
    }
    return err;
}

int PreparedModel::execute()
{
    int error = NNA_ERROR_CODE(OP_FAILED);
    if (graph_) {
        //vsi_nn_PrintGraph(graph_);
        vsi_status run_state = vsi_nn_RunGraph(graph_);

        if (VSI_SUCCESS == run_state) {
            error = NNA_ERROR_CODE(NO_ERROR);
            //vsi_nn_DumpGraphNodeOutputs(graph_, "nodes", nullptr, 0, false, VSI_NN_DIM_FMT_NCHW);
        } else {
            NNRT_LOGE_PRINT("Process complete state: %d.", run_state);
        }
    }
    return error;
}

int PreparedModel::setInput(uint32_t index, const void* data, size_t length)
{
    //bool input_mapped_as_vx_tensor = tensor_mapping_.find(index) != tensor_mapping_.end();

    //if (!input_mapped_as_vx_tensor) {
    //    NNRT_LOGD_PRINT("Should not set input for no-value optional parameter");
    //    return NNA_ERROR_CODE(OP_FAILED);
    //}

    //index = tensor_mapping_[index];

    if (index >= graph_->input.num) {
        return NNA_ERROR_CODE(OP_FAILED);
    }
    vsi_nn_tensor_t* tensor = vsi_nn_GetTensor(graph_, graph_->input.tensors[index]);
    if (!tensor) {
        // TODO: Optional tensor.
        return NNA_ERROR_CODE(NO_ERROR);
    } else if (!data || !length) {
        return NNA_ERROR_CODE(OP_FAILED);
    } else {
        uint32_t tensor_size = vsi_nn_GetTensorSize(tensor->attr.size, tensor->attr.dim_num,
                tensor->attr.dtype.vx_type);
        if (length != tensor_size) {
            NNRT_LOGE_PRINT("Tensor size mismatch %u vs %u.", tensor_size, length);
            return NNA_ERROR_CODE(OP_FAILED);
        }
        vsi_nn_CopyDataToTensor(graph_, tensor, (uint8_t*)data);
    }
    return NNA_ERROR_CODE(NO_ERROR);
}

int PreparedModel::getOutput(uint32_t index, void* data, size_t length)
{
    if (index >= graph_->output.num || !data || !length) {
        return NNA_ERROR_CODE(OP_FAILED);
    }
    vsi_nn_tensor_t* tensor = vsi_nn_GetTensor(graph_, graph_->output.tensors[index]);
    if (!tensor) {
        return NNA_ERROR_CODE(OP_FAILED);
    } else {
        uint32_t tensor_size = vsi_nn_GetTensorSize(tensor->attr.size, tensor->attr.dim_num,
                tensor->attr.dtype.vx_type);
        if (length != tensor_size) {
            NNRT_LOGW_PRINT("Tensor size mismatch %u vs %u.", tensor_size, length);
            return NNA_ERROR_CODE(OP_FAILED);
        }
        if (length != vsi_nn_CopyTensorToBuffer(graph_, tensor, (uint8_t*)data)) {
            return NNA_ERROR_CODE(OP_FAILED);
        }
    }
    return NNA_ERROR_CODE(NO_ERROR);
}

int PreparedModel::updateOutputOperand(uint32_t index, const op::OperandPtr operand_type)
{
    uint32_t operand_index = model_->outputIndex(index);
    return model_->updateOperand(operand_index, operand_type);
}

int PreparedModel::updateInputOperand(uint32_t index, const op::OperandPtr operand_type)
{
    uint32_t operand_index = model_->inputIndex(index);
    return model_->updateOperand(operand_index, operand_type);
}

}