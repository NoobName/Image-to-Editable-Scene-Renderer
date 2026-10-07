# Reference lighting proposals (stage 30)

References propose target illumination in the fixed source camera frame. They never replace source pixels, geometry, albedo, source calibration, Exposure or Look. Different-content references transfer fitted illumination statistics, not pixel RGB. Same-scene is a caller declaration, not automatic registration.

```powershell
python tools/reconstruction/reference_examples.py --output generated/reference-examples
python tools/reconstruction/match_reference.py generated/reference-examples/reference --source-package generated/reference-examples/source --output generated/reference-analysis --relation same-scene
./build/Debug/ImageSceneRenderer.exe --package generated/reference-examples/source --reference-proposal generated/reference-analysis --apply-reference --work-mode image --ui
```

Use the existing reconstruction Conda environment. UI **Reference** accepts an image or saved package, creates an offline Python process, then shows a preview, fit residual, direction convention, confidence and target differences. **Apply target proposal** and **Restore target before reference** share the CLI/state validator. Raw images use configured cached MoGe/Marigold adapters, or explicit dummy/neutral fallbacks; no downloads or new models. Saved packages reuse numeric observations. Unidentifiable/neutral estimates cannot be applied as recovered illumination.

`--reference-input PATH` runs the process; `--reference-proposal DIR_OR_JSON` loads a saved proposal; `--reference-relation same-scene|different-content` selects semantics; `--apply-reference` explicitly applies; `--reset-reference` restores the prior target after applying. `--reference-dummy` chooses labelled fallbacks. Test switches `--reference-repeat 1..5` and `--reference-cancel-frame N` exercise lifecycle. Existing smoke flags remain compatible.

`reference.json` version 1 follows `schemas/reference-analysis.schema.json`: source ID/hash and source calibration snapshot/revision, reference identity/camera/analysis provenance, target proposal, confidence/unavailable fields, fit diagnostics and hashed relative preview assets. C++ validates schema, supported claims, normalized direction, ranges, paths, hashes and sizes before publication. Source calibration changes invalidate a pending proposal. Scene revision changes discard background completion. Recipe stores the applied parameters, so replay does not need the reference job directory; it does not reopen reference UI history.

Illumination uses `B + D * max(dot(N,-travel),0)` with LH camera normals. RGB coefficients are fitted from intrinsic albedo, bounded in chromaticity, then normalized to preserve source `luminance(D+B)`; Exposure is fixed. Direction copies reference-camera XYZ into source-camera XYZ by explicit convention, not a shared world sun. Absolute radiance, environment maps and relative exposure remain unavailable.

Reference previews are labelled analysis previews (max 1024 edge), not canonical anchors. Two fixed ImGui slots 61/60 are replaced only after a fence flush; source/viewport slots 62/63 remain separate. A preview replacement may briefly wait for the GPU; fitting runs off the main thread. No new relighting shader or optimization algorithm is added here.

Validation: `tools/Validate-Reference.ps1`, `tools/reconstruction/verify_reference.py`, `ReferenceTests`, `test_reference.py`, `validate_reference_contract.py`. Real ambiguous photos are reported as unavailable even if residual decreases. Confidence is a heuristic, not a calibrated probability.
