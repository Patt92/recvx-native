# Quality of the Original-Engine Port

Binding for all porting work in accordance with `AGENTS.md`.
The goal is the complete original engine. Approximations, replacement logic and
special-case fixes invented after the fact are forbidden.

## Coverage is not proof of quality

`python3 tools/port_check.py` records the demonstrated port coverage by area and
original function. Even extracted original code can have different semantics due
to generator changes, wrong inputs or missing services. Percentages are therefore
no proof of behavior faithful to the original.

## Verification method for every port

1. Before changes, read `git status --short` and preserve ongoing work by others.
2. Find the responsible original mechanism: function, callers, dispatch,
   tables and data. Consult the disassembly when the semantics are unclear.
3. Compare original and port, including generator and host binding. Do not stop
   at the name of an original function or at a copied function body. Check the
   path that actually executes down to its services.
4. Compare states, flags, call order, frame progression, interpolation, timers,
   random calls, resource selection and side effects. Also account for init,
   release, cancellation, room change and re-entry.
5. Look in particular for fixed return values, empty functions, heuristics, host
   phases, fixed time limits, synthetic poses, blanket blending and filtering by
   room/object ID. Hits are indications; only comparison with the original proves
   a deviation. Adopt original special cases together with their source location.
6. Limit platform adaptations to ABI, data representation and technical interfaces.
   A different decision or a different state transition is not a platform
   adaptation. Debug overrides must originate in the debug state; original
   routines must not carry debug defaults of their own.
7. Document deviations with both source locations and the concrete behavioral
   consequence. Distinguish proven defects, open bindings and unresolved semantics.
   Do not derive a deviation from an outdated comment alone.
8. Port the shared original mechanism and its dependencies. Do not add a new
   replacement solution. Treat open original services explicitly as gaps and do
   not cover them with a supposedly harmless default value.
9. Verify specifically against original state transitions and data. Checks must be
   able to expose wrong bindings, not merely retell the new code. Mark test setups
   and debug overrides as such. Do not adopt a fixed host tick count as an
   expectation when that count is precisely the deviation.
10. In the result, state original source locations, changes, checks and remaining
    limits. A build, screenshot or single replay proves neither a complete port
    nor a complete playthrough.

## Proven original defects (user decision, 2026-10-23)

A defect in the original code may be fixed, and the mechanism then counts as
cleanly ported, when all of the following hold:

1. **Proven defect.** Original source, and disassembly where the compiled code
   could differ, shows an access outside the object or table, a read of an
   uninitialized variable or register, a division by zero, or a NULL dereference.
   A claim in a comment or report is not proof (`AGENTS.md`: no correction
   without evidence).
2. **Harmful outcome.** The defect can make the original crash, behave
   differently from run to run (the result depends on unrelated memory, stack or
   register contents), or show a visible glitch (a garbage pose, image, color or
   position), even if that glitch is deterministic. A defect whose result is
   defined and harmless stays faithful: for example a read that lands on a valid
   neighbouring element of the same table, or a register the disassembly shows is
   always set by the preceding code to a sensible value.
3. **Reachable.** The path can run with the shipped data, or the triggering state
   can arise in play. Unreachable defects stay documented gaps; do not fix them
   speculatively.

The fix must be the smallest local guard and must not change behavior in the
in-range case. Choose the replacement in this order and state which rule applies:

1. Take the branch the routine already has for the absent case (no target, no
   zone, no link, no work, no material).
2. Drop an out-of-range write, and drop a draw whose source image was never
   written (for example a render texture before its first render).
3. For a read outside an array: if it indexes constant original data (a frame,
   key or table entry), clamp to the nearest valid element of that data (the
   last one past the end, the first one before the start); if it
   indexes a buffer the routine allocates and initializes itself, use that
   initial value.
4. For an uninitialized variable, use the constant the same routine assigns to
   it on all of its other paths. If those paths compute it (random, position,
   timer) or disagree: when the value is only stored into persistent state, skip
   that store so the state keeps its previous value; when it must be passed to a
   call the original makes anyway (with its side effects), pass zero.
5. For a division by zero, use the zero-length case the routine handles
   elsewhere; otherwise the quotient is zero.

A fix never adds calls with side effects (random numbers, allocations, sounds,
effects), so the original order of those calls stays intact. Never introduce a
new mechanic, tuning value, room or object ID special case, or a value chosen
because it "looks right". Mark each fix in code with
`ORIGINAL-FIX (QUALITY.md):` followed by the original location, the defect, the
rule number and the resulting behavior. Remove the corresponding gap report,
record the fix in the ledger note, and add a test that drives the defect case to
the fixed result plus a check that the in-range behavior is unchanged.

Out of scope: host limitations (for example renderer domains), unported or
host-replaced features, and original behavior without a proven defect of the
kinds above, which stays faithful even where it looks odd.
