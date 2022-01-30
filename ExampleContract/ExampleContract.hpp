#pragma once

#include <ContractApi.hpp>

#include <graphene/chain/evaluator.hpp>

class ExampleContract : public graphene::chain::evaluator<ExampleContract> {
public:
    // Part of graphene::chain::evaluator interface
    using operation_type = graphene::chain::custom_operation;
    // Rename it to comply with local naming semantics
    using Operation = operation_type;

    ExampleContract();

    // ContractApi interface
    graphene::protocol::void_result do_evaluate(const Operation& op) {
        ilog("Got an op: ${OP}", ("OP", op));
        return {};
    }
    graphene::protocol::void_result do_apply(const Operation& op) {
        return {};
    }
};

bool registerContract(graphene::chain::database& db, uint8_t spaceId) {
    ilog("Registering contract evaluator");
    db.register_evaluator<ExampleContract>();

    return true;
}
