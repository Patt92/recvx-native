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
