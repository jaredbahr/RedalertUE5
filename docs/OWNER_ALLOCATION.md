# Human owner allocation

This narrow slice of [issue #11](https://github.com/jaredbahr/RedalertUE5/issues/11) prevents server-assigned human identities from colliding with the current reserved bot owner100. It is based on native draft #9, independently of the tested controls draft #14. Main and those existing heads must remain unchanged.

The GameMode owns a monotonic allocator for its lifetime. Human identities remain positive int32 values starting1,2; reservation100 is skipped. A wider internal cursor represents exhaustion after MAX_int32 without overflow or reuse. Invalid/exhausted attempts clear the output to INDEX_NONE. On exhaustion PostLogin destroys the newly admitted controller and returns before adding model player, resources or units. The RPC still derives its owner from the server controller. The bot reservation is named centrally and used by the native bot/fixture adapter.

Focused native OwnerAllocation tests cover99->101, a reserved initial candidate,256unique allocations, unchanged bot ownership/resources/order/sequence, final two valid IDs, repeated exhaustion and nonpositive starts. The existing four command groups remain required. The headless runner requires all five exact groups and records source SHA, dirty status and timing; it uses process-local D: cache/temp paths, a90second deadline and CoreLimit2. Build remains max3parallel actions/NoUBA; root separately verifies only owned processes at Normal priority/two-CPU affinity. Actual results must be recorded before claiming validation.

This does not implement authentication, disconnection cleanup, reclaiming/rebinding an existing identity, reconnect policy, resource retention/reset policy, session persistence or internet ingress limits. IDs are not reused within a GameMode lifetime. The current spawn-position formula remains a tiny fixture and these allocation tests are not hundreds of real connections, eight-player or army-performance evidence. Exhaustion admission behavior is source-reviewed; the focused model tests do not open MAX_int32 network sessions. Issue #11 remains open for the broader lifecycle work; #12 remains the ingress follow-up. No rendering/asset changes or art approval are part of this patch.

## Evidence

Pending installed-toolchain build, five-group native execution and independent review. Generated evidence is ignored under generated/; do not upload binaries.