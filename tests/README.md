# Save cleanup regression test

`MaintainDecapitations` processes stale head IDs after loading serialized data.
Erasing a `headToActorMap` node invalidates its iterator. Reading `actorIt->second`
after that erase is a use-after-free, even if the old memory happens to retain
the actor ID on a particular run. Copy the ID before erasing the node.

The test runner extracts the actual final cleanup loop from
`src/Serialization.hpp`, rather than keeping a second implementation of it. It
compiles that loop with the same standard container types and a minimal form-ID
validity predicate. No game, plugin loader, saves, or network access is needed.

In an x64 Visual Studio developer shell with Python 3:

```powershell
python tests/run_cleanup_tests.py --asan
python tests/run_cleanup_tests.py
```

GCC/Clang can also be used:

```sh
python3 tests/run_cleanup_tests.py --compiler clang++ --asan
```

To demonstrate the regression, save the upstream `Serialization.hpp` to a
temporary file and pass it using `--source <path> --asan`. AddressSanitizer
reports a heap-use-after-free at the read following `erase`. With the fix, the
same harness passes empty/missing entries, removal from both maps, preservation
of unrelated mappings, duplicate cleanup requests, invalid origin IDs, a
4,096-entry batch, and 1,000 repeated cleanup cycles.

This is an isolated lifetime and container-behavior test. It does not exercise
SKSE callbacks, resolve in-game references, or validate gameplay/save loading.
Gameplay validation remains pending. No raw game dumps or personal save data
are included.
