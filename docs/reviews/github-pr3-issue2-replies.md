# Comment drafts — PR 3 and issue 2

These are drafts, not posted comments. Review target: PR head `66a92a3d8e25fafb7f30c4f09c80f0cc6fffb5b6`, trial-merged into main `cdc1af0`.

## PR 3 — suggested reply

Thanks for the contribution and for the detailed device testing. I trial-merged this into the current main branch without conflicts, built the merged macOS and iOS Simulator targets, and ran the host tests. The touch layout/ownership tests pass, and I also launched the merged app on an iPhone simulator: it reaches the title screen with the portrait split layout and the SDL virtual touch controller connected.

The virtual-controller approach fits the existing input/rebinding system well, and the separation between layout logic and the UIKit overlay makes this straightforward to test. I did not find a merge-blocking issue in the reviewed changes.

My checks do not cover real-device haptics, physical-controller connect/disconnect, multitouch gameplay or fold/rotation transitions. Your reported hands-on tests are useful coverage for those; I have not independently reproduced them. The existing RM_0110 regression test fails on main because it rejects snapshot format 3, not because of this PR. There are no GitHub CI results attached to this PR, so the local build/test results are the verification available here.

One non-blocking UX point: in portrait, connecting a physical controller hides the control layers, but the picture still uses the split layout, leaving the lower half empty. It would be worth deciding whether it should expand back to the full available view in that case.

## Issue 2 — suggested reply

Thanks for reporting this across several platforms. I have implemented an optional **Display gamma** setting in OPTION and in the debug menu, saved in `video.ini`. It offers **1.0 (unchanged), 1.7 and 2.2**. The setting is applied to the final RGB image through a shared output path, so it does not depend on a particular GPU backend; alpha and game logic are unchanged.

The default remains 1.0, which is an exact bypass. Higher values lift midtones using `output = input^(1/gamma)`, preserving black and white. These are display-adjustment presets, not an assertion that the Gameshark settings use precisely the same transfer curve.

The pixel and configuration tests pass, including changing and saving the setting without a GPU presentation stage. This is currently a local implementation, not yet a published release. I have not confirmed a single underlying rendering defect that explains every dark-image report, and would appreciate screenshots and your preferred setting once a build containing this option is available.
