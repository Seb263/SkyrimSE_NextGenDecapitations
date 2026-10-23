# Cleanup lifetime validation

Upstream baseline: `f15fdb17726c9d389bc8e4a88db9c9ee1d8ef208` (1.4.4).
Test date: 2026-10-23. Windows x64, MSVC 19.44 / Visual Studio 2022.

`MaintainDecapitations` erases a node from `headToActorMap`, then reads the
invalidated iterator to obtain the actor ID for reverse-map cleanup. The fix
copies that ID before erasing the node. It does not change the serialized data
format, SKSE callbacks, reference removal, settings, or the conditions that
select stale entries.

The regression runner extracts that exact production loop into a standalone
fixture. Using the baseline header with `--source ... --asan` failed with exit
code 1. The relevant sanitizer findings, summarized without local paths or
process-specific addresses, were:

```text
ERROR: AddressSanitizer: heap-use-after-free
READ of size 4 in Cleanup, generated cleanup.cpp:23
  actorIt->second
freed by: unordered_map::erase in Cleanup, generated cleanup.cpp:22
  headToActorMap.erase(actorIt)
```

The same fixture with the fixed header passes with and without AddressSanitizer:

```text
PASS: empty, missing, single, selective, duplicate, absent reverse, invalid origin,
4096-entry batch, and 1000 repeated cleanup cycles
```

The complete x64 plugin also builds successfully in `releasedbg` mode with the
repository's pinned CommonLibSSE-NG submodule and SE/AE/VR support enabled. Its
`SKSEPlugin_Query` accepts a Skyrim 1.5.97 / SKSE 2.0.20 interface in an isolated
probe. This probe does not call `SKSEPlugin_Load`, install hooks, or launch Skyrim.

Gameplay validation is pending. The standalone reproducer proves the iterator
lifetime bug; it does not establish that this bug caused any particular crash
in a modded game. No game dumps, saves, or personal diagnostics are published.
