# Desktop reconstruction

Prompt 14 connects the Python pipeline to the DX12 editor through an independent process and files. No Python DLL, service or agent is used.

## Use

Start `build/Debug/ImageSceneRenderer.exe`. Choose **File > Reconstruct Image...**, select a JPEG/PNG, and reconstruction starts automatically. The current scene remains interactive. After export, CPU loading and GPU preparation run in the background; a completed scene replaces the current scene at a frame boundary.

The progress window uses explicit stage tables: full reconstruction shows geometry, segmentation, materials, lighting and export; offline lighting shows lighting and export. Overall states are Idle / Processing / Loading / Ready / Error. All Complete rows mean Python has finished; `Loading` includes decoding and GPU preparation. Close the status window after Ready, or reopen it using **File > Reconstruction status**.

Defaults: MoGe, SAM 2 automatic masks, Marigold, offline, maximum image edge 512. Automatic names use existing geometry heuristics, without named semantic prompts. Material normals retain the explicit fallback described in [Material.md](Material.md).

**File > Reconstruction settings...** selects `python.exe`, backends, image size and offline mode. `dummy / dummy / neutral` is a lightweight connection test. Dependencies and models are installed through the existing setup scripts; the application does not silently install packages. Turning offline off lets the existing adapters fetch missing weights.

Interpreter selection: a saved path in `generated/reconstruction-settings.json`, the named Conda environment from `%USERPROFILE%/.conda/environments.txt`, or the project `.venv`. `ISR_PYTHON` overrides discovery/saved settings; `--reconstruction-python` overrides it for the current run. Only the interpreter path persists; backend choices and size are session settings. This computer automatically finds `D:\miniconda\envs\image-scene-renderer\python.exe`. Conda activation is unnecessary: the child receives that environment's executable/DLL search paths.

## Output and recovery

```text
generated/
  <image-stem>-<process-id>-<timestamp>/
    scene.json
    meshes/ textures/ objects/ masks/ debug/
  .reconstruction-jobs/<job-id>/
    progress.json
    python.log
  reconstruction-settings.json
```

Every run uses a new directory. The status window shows paths and a Copy log path button. Cancel terminates the managed Python process tree. Errors retain the current scene and offer Retry / Settings. Cancellation during GPU preparation waits for the private upload to finish before discarding it; submitted GPU work is not forcibly destroyed. Closing the app cancels Python. Logs and written files remain; a forcibly stopped export may leave an incomplete staging directory, which is never imported.

## Protocol and threading

Optional CLI flags `--progress-file <path>` and `--job-id <id>` publish a versioned snapshot outside the output package. Standalone CLI use needs neither flag.

Prompt 19 writers use version 2 and a `stage_order` array, either `["geometry","segmentation","materials","lighting","export"]` or `["lighting","export"]`. The reader validates the complete table before publishing a snapshot. The historical version 1 example below remains supported. **Fit saved observations** and **Export calibrated source** reuse the same manager, worker process, cancellation and GPU commit path; see [LightingBaseline.md](LightingBaseline.md). `--fit-lighting <package>` runs the offline job from the renderer CLI.

```json
{
  "version": 1,
  "job_id": "example-job",
  "sequence": 10,
  "state": "complete",
  "stages": {
    "geometry": "complete", "segmentation": "complete",
    "materials": "complete", "export": "complete"
  },
  "detail": "ScenePackage published"
}
```

Python writes a temporary file and replaces the snapshot. Windows readers allow shared deletion; Python briefly retries sharing conflicts. C++ checks identity, version, sequence, stage values, process exit status and a final complete snapshot before loading. It does not parse console progress bars or invent time-based percentages. Geometry refers to prediction; Scene Export includes grid mesh construction and file publication.

`ReconstructionManager` owns a worker, mutex-protected status snapshots and a CPU ScenePackage. stdout/stderr go directly to a file to avoid pipe back-pressure. `CreateProcessW` uses an explicit executable and quoted arguments without a shell. A restricted handle list and kill-on-close Job Object manage child ownership. See Microsoft's [process creation](https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes) and [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects) documentation.

`PreparedScene` owns new descriptor heaps, a command queue/list/allocator and fence. Texture/mesh uploads, PSOs and shadows are prepared on a worker. The main thread adopts only a finished result; previous GPU resources stay alive until their graphics fence completes. Environment views are created in the new scene heap, with sky/post-processing bindings kept consistent. A failed preparation preserves the old scene.

## Verification

```powershell
.\tools\Build.ps1 -VisualStudioPath D:\VisualStudio
conda activate image-scene-renderer
python tools/reconstruction/test_reconstruction.py --validator build/Debug/ValidateScenePackage.exe --validator build/Release/ValidateScenePackage.exe
.\tools\Run-ReconstructionSmoke.ps1 -Image "input.jpg" -Preset dummy -UI
.\tools\Run-ReconstructionSmoke.ps1 -Image "input.jpg" -Preset full -UI -LogName my-full-run
```

Renderer CLI also accepts `--reconstruct "input.jpg"`, `--reconstruction-python ".../python.exe"`, and `--reconstruction-preset full|dummy`. With reconstruction, finite `--frames N` means N frames after Ready/Error, while frames continue during Processing/Loading. Diagnostics include `--reconstruction-repeat 2` and `--reconstruction-cancel-frame 20`. Only finite tests have a 20-minute timeout.

Native file selection runs on a separate dialog thread. Automated tests exercise the same manager through CLI; they do not claim manual interaction with the Windows picker. Chinese tutorials, Q&A and detailed results stay under local, gitignored `docs/`.
