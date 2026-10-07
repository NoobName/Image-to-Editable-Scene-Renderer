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
