# SnAPI::GameFramework::OptionalTickContractConcept

Compile-time concept satisfied by types exposing at least one recognized tick phase.

This concept is useful for generic helpers that only care whether a type participates in any engine tick phase at all, without requiring the full node/component contract.
