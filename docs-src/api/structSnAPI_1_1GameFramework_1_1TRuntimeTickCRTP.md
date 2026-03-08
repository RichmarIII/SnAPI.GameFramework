# SnAPI::GameFramework::TRuntimeTickCRTP

Marker CRTP base for dense runtime objects that participate in the generic runtime-phase system.

The base is marker-only. It does not provide virtual hooks or storage. Lifecycle functions are discovered directly on `TDerived` via the surrounding concepts.
