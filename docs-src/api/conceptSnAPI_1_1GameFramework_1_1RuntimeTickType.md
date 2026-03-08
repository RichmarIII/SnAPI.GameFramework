# SnAPI::GameFramework::RuntimeTickType

Compile-time contract for types eligible to live in `TDenseRuntimeStorage`.

A valid runtime type must satisfy `NonPolymorphicRuntimeType` and opt into one of the marker CRTP families. Optional lifecycle hooks such as `OnCreate`, `Tick`, `FixedTick`, `LateTick`, and `PostTick` are then detected automatically.
