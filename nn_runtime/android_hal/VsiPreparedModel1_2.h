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
#ifndef ANDROID_ML_NN_COMMON_OPENVX_EXECUTOR_H
#define ANDROID_ML_NN_COMMON_OPENVX_EXECUTOR_H
#include <vector>

#include <nnrt/model.hpp>
#include <nnrt/types.hpp>
#include <nnrt/compilation.hpp>
#include <nnrt/execution.hpp>
#include <nnrt/op/public.hpp>
#include <nnrt/file_map_memory.hpp>
#include "HalInterfaces.h"
#include "ExecutionBurstServer.h"
#include <ui/GraphicBuffer.h>

#include "Utils.h"
using android::sp;

namespace android {
namespace nn {
namespace vsi_driver {
    /*record the info that is gotten from hidl_memory*/
    struct VsiRTInfo{
        sp<IMemory>         shared_mem;             /* if hidl_memory is "ashmem", */
                                                    /* the shared_mem is relative to ptr */
        size_t                buffer_size;
        std::string         mem_type;               /* record type of hidl_memory*/
        uint8_t *           ptr;                    /* record the data pointer gotten from "ashmem" hidl_memory*/
        std::shared_ptr<nnrt::Memory>  vsi_mem;   /* ovx memory object converted from "mmap_fd" hidl_memory*/
        sp<GraphicBuffer> graphic_buffer;
    };

    class VsiPreparedModel : public V1_2::IPreparedModel {
   public:
    VsiPreparedModel(const V1_2::Model& model):model_(model) {
        native_model_ = std::make_shared<nnrt::Model>();
        Create(model);
        }

    ~VsiPreparedModel() override {
        release_rtinfo(const_buffer_);
        }

    Return<ErrorStatus> execute(const Request& request,
                                const sp<V1_0::IExecutionCallback>& callback) override{
        // TODO: do asynchronously execute
        return executeBase(request, MeasureTiming::NO, callback);
    };

    Return<ErrorStatus> execute_1_2(const Request& request, MeasureTiming measure,
                                    const sp<V1_2::IExecutionCallback>& callback) override {
        // TODO: do asynchronously execute
        return executeBase(request, measure, callback);
    };

    Return<void> executeSynchronously(const Request& request, MeasureTiming measure,
                                      executeSynchronously_cb cb) override {
        executeBase(request, measure, cb);
        return Void();
    };

    Return<void> configureExecutionBurst(
            const sp<V1_2::IBurstCallback>& callback,
            const android::hardware::MQDescriptorSync<V1_2::FmqRequestDatum>& requestChannel,
            const android::hardware::MQDescriptorSync<V1_2::FmqResultDatum>& resultChannel,
            configureExecutionBurst_cb cb) override;

   private:
        /*create ovxlib model and compliation*/
        Return<ErrorStatus> Create(const V1_2::Model& model);

        void fill_operand_value(nnrt::op::OperandPtr ovx_operand, const V1_2::Operand& hal_operand) ;
        void construct_ovx_operand(nnrt::op::OperandPtr ovx_oprand,const V1_2::Operand& hal_operand);
         Return<ErrorStatus> map_rtinfo_from_hidl_memory(const hidl_vec<hidl_memory>& pools,
            std::vector<VsiRTInfo>& rtInfos);
        void release_rtinfo(std::vector<VsiRTInfo>& rtInfos);

        template <typename T_IExecutionCallback>
        Return<ErrorStatus> executeBase(const Request& request,
                                              MeasureTiming measure,
                                              const T_IExecutionCallback& callback);

        const V1_2::Model model_;
        std::shared_ptr<nnrt::Model> native_model_;
        std::shared_ptr<nnrt::Compilation> native_compile_;
        std::shared_ptr<nnrt::Execution> native_exec_;

        /*store pointer of all of hidl_memory to buffer*/
        std::vector<VsiRTInfo> const_buffer_;
        std::vector<VsiRTInfo> io_buffer_;
};
}
}
}
#endif