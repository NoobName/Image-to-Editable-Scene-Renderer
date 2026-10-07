# Model and demonstration provenance

Checked 2026-10-06 against existing pinned adapters and official cards. No new weights,
models or training are introduced by stage 32. C++ consumes only validated file contracts.

| Adapter | Pinned checkpoint revision | Outputs / limits | Official terms |
|---|---|---|---|
| [MoGe-2 ViT-S normal](https://huggingface.co/Ruicheng/moge-2-vits-normal) | `26b477f41595707c5db6770294c0d1721e8ed4ed` | depth, points, geometry normals, intrinsics, binary validity; not complete geometry | [official code and license](https://github.com/microsoft/MoGe); MIT code/card, examine pinned assets before redistribution |
| [SAM2.1 Hiera Tiny](https://huggingface.co/facebook/sam2.1-hiera-tiny) | `de431c4043854a71d8101e17995dfe596bf101a5` | masks; names are prompts/heuristics, not model semantic labels | Apache-2.0 |
| [Marigold IID Appearance](https://huggingface.co/prs-eth/marigold-iid-appearance-v1-1) | `e7280a0a0fc5a0df0b36050882b3d8b77da22fd9` | albedo sRGB decoded to linear, roughness/metallicity linear; tangent normal fallback | Apache-2.0 code / CreativeML Open RAIL++-M weights |
| [Marigold IID Lighting](https://huggingface.co/prs-eth/marigold-iid-lighting-v1-1) | `08c3930bb641abf786ba44ce92547507ebefbc16` | linear albedo/shading/nonnegative residual, native gauge; residual not certified specular | Apache-2.0 code / CreativeML Open RAIL++-M weights |

MoGe source revision `b942f00bdc2a2a23ebb474fbe034d487e6dcceec`; SAM source
`2b90b9f5ceec907a1c18123530e92e794ad901a4`. Full SHA-256 weight/config checks are in
`tools/reconstruction/pipeline/adapters/{moge_config,sam2_config,marigold_config,intrinsic_config}.py`.
Exported metadata records the actual backend/revision/settings. Do not replace a model card's
use restrictions with “open weights means unrestricted”. Code, weights and input photos have
separate rights. The project has not audited arbitrary user content for redistribution.
MoGe's bundled DINOv2 portion has Apache-2.0 terms; the enclosing MIT license does not
replace dependency licenses. The normal-checkpoint card has MIT metadata but little prose.

## Demo inputs and evidence roles

| Category | Source | Rights and what it demonstrates |
|---|---|---|
| Diffuse indoor + mirror/window/TV stress | MoGe `example_images/01_HouseIndoor.jpg` at the pinned source commit | photo-specific redistribution terms not separately established; local evaluation only, not bundled as a licensed public dataset; real four-adapter rerun in stage32 |
| Outdoor / sky and highlights | MoGe `03_Traffic.jpg`, [pinned source](https://github.com/microsoft/MoGe/tree/74fbce054ebed49800de42d0ad0e83495065719a/example_images) | photo-specific terms not established; stage23 saved Lighting inference with Dummy geometry/neutral material; labelled cached in stage32 |
| Painting | Van Gogh, *Wheat Field with Cypresses* (1889), [Met object 436535](https://www.metmuseum.org/art/collection/search/436535) | Met explicitly marks this image Public Domain; cached Lighting analysis is not proof of physically correct paint illumination |
| Fine detail | `ratio_examples.py` | procedural 1500×1000 fine text/one-pixel texture, explicit Dummy plane with hole and manual light; tests identity, not learned geometry |
| Specular | `specular_examples.py` | procedural analytic GGX sphere/plane; not glass/refraction or unrestricted mirror reflection |
| Deep shadow / failure | `cast_shadow_examples.py`, `stability_examples.py` | project-authored black, emission, missing occluder, low-support and saturated diagnostics; checks bounded protection |
| Known target/reference | `reference_examples.py` | analytic multi-material Lambert field with independent target direction, plus degenerate plane; validates recovery separately from no-op |

`core_demo_inputs.py` regenerates the project-authored set and writes per-file hashes and
source descriptions. Local `generated/prompt23-inputs/sources.json` retains downloaded URLs
and hashes; weights and downloaded images stay ignored. For a distributable demo, regenerate
the synthetic set or provide rights-cleared photos yourself. Do not publish the whole local
generated directory on the assumption that the renderer's code terms cover its photos.

Offline commands, visual sequence and quality matrix are in [CoreDemo.md](CoreDemo.md).

## Optional stage 34 research

Rechecked 2026-10-07 against official implementations, rather than assuming a text-to-image
model accepts the renderer's guidance. Only PIXLRelight was downloaded and actually run.

| Candidate | Inspected revision | Suitability / decision |
|---|---|---|
| [PIXLRelight](https://github.com/mlfarinha/pixlrelight) | code `f8cb2dba4d08b3dfcd392cf9b2946a11f1f48c23`, [weights](https://huggingface.co/mlfarinha/pixlrelight/tree/5def370459c15ee36c524068bb8338b3f6c6d6c0) `5def370459c15ee36c524068bb8338b3f6c6d6c0` | Source RGB + target intrinsic A/S/R; real optional adapter tested at 256 on 8 GB GPU. The physics export supplies target RGB. |
| [IC-Light](https://github.com/lllyasviel/IC-Light) | `bcf3f29ca85be8a4686215f477b546f5030be8b7` | Foreground+text/background; selected light preference is an initial latent ramp, not our geometric guidance. Not installed/run. |
| [DiLightNet](https://github.com/iamNCJ/DiLightNet) | `ca749e31745f5902c1266a89249374455172559c` | Source/mask/diffuse/three-specular roughness hints require additional radiance passes and a different generation pipeline. Not installed/run. |
| [PI-Light](https://github.com/ZhexinLiang/PI-Light) | `4e918eef3c0d131a9f8f809bd3e5528bac1e0ba5` | Investigated official release status; code still announced as forthcoming in inspected README. No claimed runnable adapter. |

PIXL weights/config are **CC BY-NC 4.0**; root source is MIT, with inherited per-file
DINOv3/VGGT/Marigold notices. Preserve those notices and examine their terms separately
([DINOv3 license](https://github.com/facebookresearch/dinov3/blob/main/LICENSE.md)).
Do not infer a commercial grant from the top-level MIT file. Its reused Marigold checkpoint
retains the separate CreativeML Open RAIL++-M terms above. Input photograph rights are unchanged.

PIXL `model.safetensors` is 2,563,499,816 bytes, SHA256
`69c2bd11c2f272754f7080bc33e4b049fd334ea5d596df0c475f51c93699710e`;
config SHA256 `a3ef2cada8a223aee15b2e85d6627980962ea5446242d0d8ec24791ec2e21021`.
The adapter verifies both plus vendored source hashes. The [official paper](https://arxiv.org/abs/2605.18735)
and [model card](https://huggingface.co/mlfarinha/pixlrelight) describe the method and limits;
their model-only speed is not our measured full process/decomposition/export latency.
Local actual environment is Python 3.13 / Torch 2.8.0+cu129 / Diffusers 0.35.2 / NumPy 2.5.3,
different from the upstream Python3.11/NumPy<2 recipe; this exact environment was tested,
not advertised as universal compatibility. All source/weights/caches remain ignored.
See [NeuralRefinement](NeuralRefinement.md) for opt-in setup, boundaries and replay.
