# src/command/CommandFamily.hpp

**Primary classification:** PUBLIC API

## `Command::Planner`

Canonical Planner Type associated with the Command Primitive family.

The Stage-A Serialisation prerequisite reconciliation restores the semantic Primitive-family association without redesigning the established Command Bootstrap/Runtime deployment architecture. `Planner` therefore exists as the canonical family association required by the common Primitive-family contract; family deployment planning is not expanded by this tranche.

## `Command::Family`

Canonical Primitive family tag for Command operation Types.

It declares:

- stable `Primitives::PrimitiveFamilyIdentifier Identifier` under ESPressio Type Authority 1, family-local identifier 1;
- `using Planner = Command::Planner`.

`Primitives::PrimitiveFamilyType<Command::Family>` is asserted at compile time.

Every valid `CommandType<TCommand>` requires `TCommand::Family` to be exactly `Command::Family` in addition to the generic schema-bearing `Primitives::PrimitiveType` contract.

## Ownership boundary

`Command::Family` classifies semantic Command operation Types only. Request and Response Types are ordinary independent `System::SchemaType`s and are not Command-family Primitive Types merely because they participate in an operation contract.
