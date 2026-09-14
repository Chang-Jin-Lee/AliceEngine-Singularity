// SPDX-License-Identifier: MIT
// Explicit read and command phases prevent rule scheduling from changing the observed world.
#pragma once

#include "Runtime/World.h"
#include "Verbs/Expression.h"

namespace alice::runtime {

struct RuleContext {
    EntityId self;
    u32 behaviorIndex = 0;
    u64 tick = 0;
    f64 deltaSeconds = 0;
    SourceLocation source;
};

// Implementations capture input/physics/time/local variables before evaluation and keep them fixed.
// Never poll live devices or mutate state from these calls. Values returned here own their data.
class IRuleSymbols {
public:
    virtual ~IRuleSymbols() = default;
    virtual Result<doc::Value> Read(std::string_view name, const WorldReadView& world,
                                   const RuleContext& context) const = 0;
    virtual Result<doc::Value> Call(std::string_view name, std::span<const doc::Value> arguments,
                                   const WorldReadView& world, const RuleContext& context) const = 0;
};

Result<bool> EvaluateCondition(const verbs::Expr& expression, const WorldReadView& world,
                               const RuleContext& context, const IRuleSymbols& symbols);

// World provenance is deliberately excluded from sort order so reruns can have identical ordering.
// Document indices are zero-based; entity fields must equal ActionCommand::self.
struct CommandOrder {
    u32 entityIndex = 0;
    u32 entityGeneration = 0;
    u32 behavior = 0;
    u32 rule = 0;
    u32 action = 0;
    auto operator<=>(const CommandOrder&) const = default;
};

struct ActionCommand {
    CommandOrder order;
    EntityId self;
    std::string verb;
    doc::Value arguments;   // Fully resolved, owned values captured during the read phase.
    SourceLocation source;
};

// An executor must leave the command's affected state unchanged when it returns an error.
// Binding availability is separate from the static core verb catalogue.
class IVerbExecutor {
public:
    virtual ~IVerbExecutor() = default;
    virtual Status Execute(World& world, const ActionCommand& command) const = 0;
};

// Release every read lease first. Preflight rejects malformed/duplicate order keys before any write.
// Sort ascending, execute sequentially, collect failures and continue. No whole-batch rollback.
// Successful writes remain; destruction remains queued for the caller's CommitDestructions barrier.
// Append diagnostics, never clear the bag. Return the first error from THIS call, or success when
// this call adds no errors; earlier entries in the bag do not change its Status.
Status ApplyCommands(World& world, std::span<const ActionCommand> commands,
                     const IVerbExecutor& executor, DiagnosticBag& diagnostics);

} // namespace alice::runtime
