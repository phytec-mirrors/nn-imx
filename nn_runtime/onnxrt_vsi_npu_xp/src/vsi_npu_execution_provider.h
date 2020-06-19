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

#pragma once
#include "core/framework/allocatormgr.h"
#include "core/session/abi_session_options_impl.h"
#include "vsi_npu_model.h"

namespace onnxruntime {

struct VsiNpuExecutionProviderInfo {
    int device_id{0};
};

// Logical device representation.
class VsiNpuExecutionProvider : public IExecutionProvider {
   public:
    explicit VsiNpuExecutionProvider(const VsiNpuExecutionProviderInfo& info);
    virtual ~VsiNpuExecutionProvider();

    std::vector<std::unique_ptr<ComputeCapability>> GetCapability(
        const onnxruntime::GraphViewer& graph_viewer,
        const std::vector<const KernelRegistry*>& /*kernel_registries*/) const override;

    virtual std::shared_ptr<KernelRegistry> GetKernelRegistry() const override;
    Status Compile(const std::vector<onnxruntime::Node*>& fused_nodes,
                   std::vector<NodeComputeInfo>& node_compute_funcs) override;

   private:
    int device_id_;
    std::vector<ModelShellPtr> model_list_;
};

}  // namespace onnxruntime