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
#include "VsiPreparedModel1_2.h"

#include <sys/mman.h>
#include <sys/system_properties.h>

#include "OperationsUtils.h"
#include "Utils.h"
#if ANDROID_SDK_VERSION > 27
#include "ValidateHal.h"
#endif
#include <nnrt/event.hpp>
#include <nnrt/error.hpp>


namespace android {
namespace nn {
namespace vsi_driver {

using time_point = std::chrono::steady_clock::time_point;
static const Timing kNoTiming = {.timeOnDevice = UINT64_MAX, .timeInDriver = UINT64_MAX};

auto now() {
    return std::chrono::steady_clock::now();
};

auto microsecondsDuration(decltype(now()) end, decltype(now()) start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
};

static Return<void> notify(const sp<V1_0::IExecutionCallback>& callback, const ErrorStatus& status,
                           const hidl_vec<OutputShape>&, Timing) {
    return callback->notify(status);
}

static Return<void> notify(const sp<V1_2::IExecutionCallback>& callback, const ErrorStatus& status,
                           const hidl_vec<OutputShape>& outputShapes, Timing timing) {
    return callback->notify_1_2(status, outputShapes, timing);
}

static void notify(const V1_2::IPreparedModel::executeSynchronously_cb& callback, const ErrorStatus& status,
                           const hidl_vec<OutputShape>& outputShapes, Timing timing) {
    return callback(status, outputShapes, timing);
}

static nnrt::OperationType op_code_mapping(V1_2::OperationType op) { // Android O 8.1 API LEVEL 27
    switch (op)
    {
#define MAP_OP(op)  \
    case V1_2::OperationType::op:{ \
    LOG(INFO)<<"add operation: "<<#op; \
        return nnrt::OperationType::op;     \
    }
        MAP_OP(ADD);
        MAP_OP(CONV_2D);
        MAP_OP(DEPTHWISE_CONV_2D);
        MAP_OP(RELU);
        MAP_OP(RESHAPE);
        MAP_OP(FULLY_CONNECTED);
        MAP_OP(SOFTMAX);
        MAP_OP(CONCATENATION);
        MAP_OP(AVERAGE_POOL_2D);
        MAP_OP(MAX_POOL_2D);
        MAP_OP(MUL);
        MAP_OP(RELU1);
        MAP_OP(RELU6);
        MAP_OP(TANH);
        MAP_OP(LOGISTIC);
        MAP_OP(FLOOR);
        MAP_OP(DEQUANTIZE);
        MAP_OP(SPACE_TO_DEPTH);
        MAP_OP(DEPTH_TO_SPACE);
        MAP_OP(L2_NORMALIZATION);
        MAP_OP(RESIZE_BILINEAR);
        MAP_OP(LOCAL_RESPONSE_NORMALIZATION);
        MAP_OP(EMBEDDING_LOOKUP);
        MAP_OP(RNN);
        MAP_OP(HASHTABLE_LOOKUP);
        MAP_OP(LSTM);
        MAP_OP(SVDF);
        MAP_OP(LSH_PROJECTION);
        MAP_OP(L2_POOL_2D);
#if ANDROID_SDK_VERSION > 27
        MAP_OP(BATCH_TO_SPACE_ND);
        MAP_OP(DIV);
        MAP_OP(MEAN);
        MAP_OP(PAD);
        MAP_OP(SPACE_TO_BATCH_ND);
        MAP_OP(SQUEEZE);
        MAP_OP(STRIDED_SLICE);
        MAP_OP(SUB);
        MAP_OP(TRANSPOSE);
#endif
#undef MAP_OP

        default:
        LOG(ERROR)<<"Unknown operation code:"<< static_cast<int32_t>(op);
            break;
    }

    return nnrt::OperationType::NONE;
};

static nnrt::OperandType operand_mapping(V1_2::OperandType code)
{
#define MAP_OPERAND(code) \
    case V1_2::OperandType::code:{\
        LOG(INFO)<<"add operand: "<<#code; \
        return nnrt::OperandType::code;\
        }

    switch (code) {
        MAP_OPERAND(FLOAT32);
        MAP_OPERAND(INT32);
        MAP_OPERAND(UINT32);
        MAP_OPERAND(TENSOR_FLOAT32);
        MAP_OPERAND(TENSOR_INT32);
        MAP_OPERAND(TENSOR_QUANT8_ASYMM);
        default:
            break;
    }

#undef MAP_OPERAND

    return nnrt::OperandType::NONE;
}
void VsiPreparedModel::release_rtinfo(std::vector<VsiRTInfo>& rtInfos){
    while(!rtInfos.empty()){
        auto &rt = rtInfos.back();
        if("mmap_fd" == rt.mem_type)
            rt.vsi_mem.reset();
        else if("hardware_buffer_blob" == rt.mem_type){
            rt.graphic_buffer->unlock();
            rt.graphic_buffer = nullptr;
        }
        rtInfos.pop_back();
    }
}
 Return<ErrorStatus> VsiPreparedModel::map_rtinfo_from_hidl_memory(const hidl_vec<hidl_memory>& pools,
            std::vector<VsiRTInfo>& rtInfos){
        rtInfos.clear();
        rtInfos.resize(pools.size());

    for(size_t i = 0; i < pools.size(); i++){
        auto & hidl_memory = pools[i];
        auto & rt = rtInfos[i];

        sp<GraphicBuffer> graphic_buffer = nullptr;
        std::shared_ptr<nnrt::Memory>  vsi_mem = nullptr;
        sp<IMemory> shared_mem = nullptr;
        uint8_t *buffer = nullptr;

        if ("ashmem" == hidl_memory.name()) {
                shared_mem = mapMemory(hidl_memory);
                assert(shared_mem);
                shared_mem->read();
                buffer =
                    reinterpret_cast<uint8_t*>(static_cast<void*>(shared_mem->getPointer()));
        }else if ("mmap_fd" == hidl_memory.name()) {
                size_t size = hidl_memory.size();
                int fd = hidl_memory.handle()->data[0];
                int mode = hidl_memory.handle()->data[1];
                size_t offset = getSizeFromInts(hidl_memory.handle()->data[2], hidl_memory.handle()->data[3]);

                vsi_mem = std::make_shared<nnrt::Memory>();
                vsi_mem ->readFromFd(size, mode, fd, offset);
        }else if ("hardware_buffer_blob" == hidl_memory.name()) {
            auto handle = hidl_memory.handle();
            auto format = AHARDWAREBUFFER_FORMAT_BLOB;
            auto usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
            const uint32_t width = hidl_memory.size();
            const uint32_t height = 1;  // height is always 1 for BLOB mode AHardwareBuffer.
            const uint32_t layers = 1;  // layers is always 1 for BLOB mode AHardwareBuffer.
            const uint32_t stride = hidl_memory.size();
            graphic_buffer = new GraphicBuffer(handle, GraphicBuffer::HandleWrapMethod::CLONE_HANDLE,
                                               width, height, format, layers, usage, stride);
            void* gBuffer = nullptr;
            auto status = graphic_buffer->lock(usage, &gBuffer);
            if (status != NO_ERROR) {
                LOG(ERROR) << "RunTimePoolInfo Can't lock the AHardwareBuffer.";
                return ErrorStatus::INVALID_ARGUMENT;
            }
            buffer = static_cast<uint8_t*>(gBuffer);
        }else{
                LOG(ERROR) << "invalid hidl_memory";
                return ErrorStatus::INVALID_ARGUMENT;
        }

        rt.shared_mem = shared_mem;
        rt.mem_type = std::string(hidl_memory.name());
        rt.ptr = buffer;
        rt.vsi_mem = vsi_mem;
        rt.buffer_size = hidl_memory.size();
        rt.graphic_buffer = graphic_buffer;
    }
    return ErrorStatus::NONE;
}

    void VsiPreparedModel::fill_operand_value(nnrt::op::OperandPtr ovx_operand, const V1_2::Operand& hal_operand) {
    switch (hal_operand.lifetime) {
        case OperandLifeTime::MODEL_INPUT:
        case OperandLifeTime::MODEL_OUTPUT:
        case OperandLifeTime::TEMPORARY_VARIABLE:
            // Skip lifetime is TEMPORARY_VARIABLE, MODEL_INPUT, MODEL_OUTPUT, or NO_VALUE
            break;
        case OperandLifeTime::NO_VALUE: {
            native_model_->setOperandValue(ovx_operand, nullptr, 0);
        } break;
        case OperandLifeTime::CONSTANT_COPY: {
            const auto& location = hal_operand.location;
            native_model_->setOperandValue(ovx_operand,
                            model_.operandValues.data() + location.offset,
                            location.length);
            break;
        } break;
        case OperandLifeTime::CONSTANT_REFERENCE: {
            const auto& location = hal_operand.location;
            auto &rt_info = const_buffer_[location.poolIndex];

            if ("ashmem" == rt_info.mem_type) {
                const uint8_t* buffer = rt_info.ptr;
                native_model_->setOperandValue(ovx_operand, buffer + location.offset, location.length);
            } else if ("mmap_fd" == rt_info.mem_type) {
                native_model_->setOperandValueFromMemory(ovx_operand, rt_info.vsi_mem.get(),
                                                        location.offset, location.length);
            }
        } break;
    }
}

void VsiPreparedModel::construct_ovx_operand(nnrt::op::OperandPtr ovx_operand,const V1_2::Operand& hal_operand) {
    ovx_operand->type = operand_mapping(hal_operand.type);
    ovx_operand->quant.scalar.scale = hal_operand.scale;
    ovx_operand->quant.scalar.zeroPoint = hal_operand.zeroPoint;
    ovx_operand->dimensions = hal_operand.dimensions;
    // tensor shape with zero should set as Null, ovx won't create concrete tensor
    // for this operand
//    for (auto d : ovx_operand->dimensions) {
//        if (0 == d) {
//            ovx_operand->setNull();
//            break;
//        }
//    }

    // TODO: add check error
    switch (ovx_operand->type) {
        case nnrt::OperandType::FLOAT32:
        case nnrt::OperandType::INT32:
        case nnrt::OperandType::UINT32:
        break;
        case nnrt::OperandType::TENSOR_FLOAT32:
        case nnrt::OperandType::TENSOR_INT32:
        case nnrt::OperandType::TENSOR_QUANT8_ASYMM: {
            break;
        }
        default:
            break;
    }
}

    Return<ErrorStatus> VsiPreparedModel::Create(const V1_2::Model& model){
        // [0] validate HAL::Model, return ErrorCode if validate failed
        // For scalar operand, dimension must be 0
        // [1] create async procedure to prepare model
        // [1.0] convert HAL model to nnrt::Model
        LOG(INFO) << __FUNCTION__;

        auto status = map_rtinfo_from_hidl_memory(model.pools, const_buffer_);
        if(ErrorStatus::NONE != status)
            return status;

        // add operand and set its value
        for (const auto& hal_operand : model.operands) {
            uint32_t registered_idx = 0;
            auto ovx_operand = native_model_->addOperand(nullptr, &registered_idx);

            construct_ovx_operand(ovx_operand, hal_operand);
            fill_operand_value(ovx_operand, hal_operand);
        }

        for (const auto& hal_op : model.operations) {
             auto ovx_op =
                native_model_->addOperation(op_code_mapping(hal_op.type) /* Operation Type*/,
                                           &hal_op.inputs[0],    /*inputs */
                                           hal_op.inputs.size(), /*num of inputs */
                                           &hal_op.outputs[0],   /*outputs */
                                           hal_op.outputs.size() /*num of outputs */
                                           );
            ovx_op->setDataLayout(nnrt::DataLayout::NHWC);
        }

//        if(preference == ExecutionPreference::FAST_SINGLE_ANSWER)
//            native_model_->relax(true);

        native_model_->finish();
        std::vector<uint32_t> inputs = model.inputIndexes;
        std::vector<uint32_t> outputs = model.outputIndexes;
        native_model_->identifyInputsAndOutputs(inputs.data(), inputs.size(), outputs.data(), outputs.size());
        native_compile_ = std::make_shared<nnrt::Compilation>(native_model_.get());
        return ErrorStatus::NONE;
    }
    Return<void> VsiPreparedModel::configureExecutionBurst(
            const sp<V1_2::IBurstCallback>& callback,
            const android::hardware::MQDescriptorSync<V1_2::FmqRequestDatum>& requestChannel,
            const android::hardware::MQDescriptorSync<V1_2::FmqResultDatum>& resultChannel,
            configureExecutionBurst_cb cb) {
            const sp<V1_2::IBurstContext> burst =
                    ExecutionBurstServer::create(callback, requestChannel, resultChannel, this);

            LOG(INFO) << " do not support Burst model temprarily";
            cb(ErrorStatus::GENERAL_FAILURE, {});
#if 0
            if (burst == nullptr) {
                cb(ErrorStatus::GENERAL_FAILURE, {});
            } else {
                cb(ErrorStatus::NONE, burst);
            }
#endif
            return Void();
            };


template <typename T_IExecutionCallback>
Return<ErrorStatus> VsiPreparedModel::executeBase(const Request& request,
                                              MeasureTiming measure,
                                              const T_IExecutionCallback& callback) {
    LOG(INFO) << __FUNCTION__;
    time_point deviceStart;
    if (measure == MeasureTiming::YES) deviceStart = now();

    map_rtinfo_from_hidl_memory(request.pools, io_buffer_);
    if(!native_exec_)
        native_exec_ = std::make_shared<nnrt::Execution>(native_compile_.get());

    std::vector<hidl_memory> io_pools = request.pools;
    std::vector<RequestArgument> input_args = request.inputs;
    std::vector<RequestArgument> output_args = request.outputs;
    std::vector<OutputShape> outputShapes(request.outputs.size());

    enum IO{INPUT = 0, OUTPUT};
    // Adjust the runtime info for the arguments passed to the model,
    // modifying the buffer location, and possibly the dimensions.
    auto update_operand = [this](const std::vector<uint32_t>& indexes,
        const hidl_vec<RequestArgument>& arguments) {
        nnAssert(indexes.size() == arguments.size());
        auto ovx_operands = native_model_->getOperands(indexes);
        for (size_t i = 0; i < indexes.size(); i++) {
            const RequestArgument& request_arg = arguments[i];
            auto ovx_operand = ovx_operands[i];
            if (request_arg.dimensions.size() > 0) {
                // It's the responsibility of the caller to validate that
                // arg.dimensions only modifies the dimensions that were
                // unspecified in the model.  That's the case in SampleDriver.cpp
                // with the call to validateRequest().
                // TODO: after revaluing the dimension, model should be re-compiled
                ovx_operand->dimensions = request_arg.dimensions;
            }
        }
    };

    // Adjust the runtime info for the arguments passed to the model,
    // modifying the buffer location, and possibly the dimensions.
    auto updateForArguments = [&](const std::vector<uint32_t>& indexes,
        const hidl_vec<RequestArgument>& arguments, IO flag) -> Return<ErrorStatus>{
        nnAssert(indexes.size() == arguments.size());
        auto ovx_operands = native_model_->getOperands(indexes);
        for (size_t i = 0; i < indexes.size(); i++) {
            const RequestArgument& request_arg = arguments[i];
            auto ovx_operand = ovx_operands[i];
            if (request_arg.hasNoValue) {
                if(flag == IO::INPUT)
                    native_exec_->setInput(i, nullptr /*operand */, nullptr, 0);
                else
                    native_exec_->setOutput(i, nullptr, nullptr, 0);
            }
            else {
                auto location    = request_arg.location;
                auto poolIndex = location.poolIndex;
                nnAssert(poolIndex < request.pools.size());

                if (flag == IO::OUTPUT){
                    outputShapes[i].dimensions = ovx_operand->dimensions;
                    outputShapes[i].isSufficient =
                                        io_buffer_[poolIndex].buffer_size >= location.length + location.offset;
                    if (io_buffer_[poolIndex].buffer_size < location.length + location.offset)
                    {
                        LOG(ERROR)<<"No Sufficient output, Execute failed";
                        notify(callback, ErrorStatus::OUTPUT_INSUFFICIENT_SIZE, outputShapes, kNoTiming);
                        return ErrorStatus::OUTPUT_INSUFFICIENT_SIZE;
                    }
                }

                auto &rt_info = io_buffer_[poolIndex];
                if("ashmem" == rt_info.mem_type){
                    uint8_t* buffer = rt_info.ptr;
                    if (flag == IO::INPUT)
                        native_exec_->setInput(i, ovx_operand, buffer + location.offset, location.length);
                    else
                        native_exec_->setOutput(i, ovx_operand, buffer + location.offset, location.length);
                }else if ("mmap_fd" == rt_info.mem_type) {
                    auto &vsi_mem = rt_info.vsi_mem;
                    if (flag == IO::INPUT)
                        native_exec_->setInputFromMemory(
                            i, ovx_operand, vsi_mem.get(), location.offset, location.length);
                    else
                        native_exec_->setOutputFromMemory(
                            i, ovx_operand, vsi_mem.get(), location.offset, location.length);
                }
             }
        }

        return ErrorStatus::NONE;
    };

    update_operand(model_.inputIndexes, input_args);
    update_operand(model_.outputIndexes, output_args);
    if (!native_model_->isCompiled()) {
        native_compile_->run();
    }

    updateForArguments(model_.inputIndexes, input_args, IO::INPUT);
    auto status =  updateForArguments(model_.outputIndexes, output_args, IO::OUTPUT);
    if( ErrorStatus::NONE != status){
        notify(callback, status, outputShapes, kNoTiming);
        return status;
    }

    int error = native_exec_->compute();
    native_exec_.reset();
    release_rtinfo(io_buffer_);

    time_point deviceEnd;
    if (measure == MeasureTiming::YES) deviceEnd = now();

    Timing timing = {.timeOnDevice = uint64_t(microsecondsDuration(deviceEnd, deviceStart)),
                     .timeInDriver = uint64_t(microsecondsDuration(deviceEnd, deviceStart))};

    LOG(INFO) << __FUNCTION__<<" time: " << toString(timing);

    if(error != NNA_ERROR_CODE(NO_ERROR)){
        notify(callback, ErrorStatus::INVALID_ARGUMENT, outputShapes, timing);
        return ErrorStatus::INVALID_ARGUMENT;
    }

    notify(callback, ErrorStatus::NONE, outputShapes, timing);
    return ErrorStatus::NONE;
}
}
}
}