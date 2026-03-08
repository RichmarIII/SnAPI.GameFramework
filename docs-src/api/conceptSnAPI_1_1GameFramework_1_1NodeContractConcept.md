# SnAPI::GameFramework::NodeContractConcept

Compile-time concept describing the API surface expected of node-like types.

`NodeContractConcept` is used by generic code that wants to interact with node objects through structure and semantics rather than concrete inheritance. `BaseNode` provides the canonical implementation, but any compatible type can satisfy this concept.

Core semantics:
- The concept requires graph identity, parenting, activation, replication, world binding, RPC, and tick-phase APIs.
- Satisfaction is checked entirely at compile time.
- This concept verifies shape only; it does not prove the runtime semantics behind those methods.
