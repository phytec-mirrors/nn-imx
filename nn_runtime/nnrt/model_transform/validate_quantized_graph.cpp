#include <vector>
#include "error.hpp"
#include "model_transform/transformations.hpp"

using namespace nnrt::op;

namespace nnrt {
int ValidateQuantizedGraph::run(Model* model, bool* modified) {
    *modified = false;
    if (nullptr == model) {
        return NNA_ERROR_CODE(NO_ERROR);
    }
    // Set bias scale and zero point
    auto operations = model->operations();
    for (auto it = operations.begin(); it != operations.end(); ++it) {
        OperationPtr op = it->second;
        switch (op->type()) {
            case OperationType::CONV_2D:
            case OperationType::DEPTHWISE_CONV_2D:
            case OperationType::DECONV_2D:
            case OperationType::FULLY_CONNECTED: {
                std::vector<op::OperandPtr> inputs = model->getOperands(op->inputs());
                if (inputs[0]->type == OperandType::TENSOR_QUANT8_ASYMM &&
                    inputs[2]->type == OperandType::TENSOR_INT32) {
                    inputs[2]->quant.scalar.scale =
                        inputs[0]->quant.scalar.scale * inputs[1]->quant.scalar.scale;
                    inputs[2]->quant.scalar.zeroPoint = 0;
                    inputs[2]->type = OperandType::TENSOR_QUANT32_SYMM;
                    *modified = true;
                }
            } break;
            default:
                continue;
        }
    }
    return NNA_ERROR_CODE(NO_ERROR);
}
}