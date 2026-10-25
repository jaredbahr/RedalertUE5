# Isolated raw CPU input correction

This is an unadopted candidate, based on `02e08abd10ddc062a0b81c6eb18eb9d939232f3d`.
It does not replace the frozen admission script, change existing bindings/pins,
or activate the approved NativeHUD runner.

`fixtures/original-admission.ps1.txt` is a byte-exact source fixture with SHA256
`f891ebaf39f09a738578ce0da9d94fa47352b27e84d8ce0fb56d447e49c5c353`.
The candidate sampler differs only inside its CPU conversion `foreach` block.
It rejects missing/null, nonnumeric, nonfinite, out-of-range and malformed named
instances before recording a numeric value. Numeric primitives from byte through
decimal are accepted; strings, booleans, chars, enums, arrays and objects are not
numeric provider data. Duplicate instance names fail closed.

Valid numeric 80 and 100 remain valid measurements and remain **ineligible** under
the unchanged `>=80` load rule. The original three queries/samples, sleeps, source
identity checks, process/game rules, RAM/disk floors, output and policy metadata
outside conversion are preserved. The 600-second sampler, reservation/lease,
qualification/native bounds and cleanup are untouched. This unrelated defect
does not explain or invalidate the earlier recorded 100% qualification failure.

The byte-exact baseline and candidate preserve the original extra blank line at
EOF. Git's whitespace check reports that inherited warning in both new copies;
the check is retained and its nonzero result must be reported, not suppressed.

## Offline fixtures

The fixture runner parses both complete source files but evaluates only the actual
CPU conversion, initialization and completeness check in an isolated in-memory
scope. It does not dot-source the sampler, mock CIM, or invoke the surrounding Git,
CIM or sleep commands. AST checks disallow commands, unexpected variables and
unapproved method calls in the evaluated region. Prefix/suffix comparisons retain
the rest of the source; four deliberate threshold/count/sleep/mask mutations must
be rejected. Missing/null fixtures must demonstrate the baseline weakness, while
valid inputs must preserve baseline numeric values and load decisions.

Fixtures cover every expected processor name, null/missing fields, malformed
types, NaN/infinities, negative/over-100 data, missing/duplicate/unknown instances,
all eleven numeric primitive types and the valid 0/79.9999/80/100 boundaries.
Precision cases reject raw Decimal values just above 100 before conversion can
round them to Double 100. A valid Decimal just below 80 that converts to Double 80
must retain the baseline's ineligible load result; the threshold is not changed.
The runner's 20-second self-check is not a hard external watchdog or universal
OS-call preemption guarantee. Execution requires independent source review,
a coordinated offline-only window, and the existing bounded owner/cleanup rules.
It is not live host admission, a NativeHUD pass or production acceptance.

No fixture has been executed while this source-only candidate is prepared.
After review and a coordinated window, the payload form is:

```powershell
& .\test-cpu-input-fixtures.ps1 -Execute -OfflineWindowConfirmed `
  -CandidateSHA256 '<exact reviewed candidate hash>' `
  -OutputDirectory 'D:\UserTemp\<fresh owned offline-result directory>'
```

Do not run the candidate sampler against the host to test this correction. Do not
retarget frozen pins or reuse an existing approval for different bytes. Adoption
and any required changes to binding/caller/controls need their own review; this
candidate and its future offline results cannot retroactively change old evidence.
