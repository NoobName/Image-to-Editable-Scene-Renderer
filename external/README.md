# Vendored dependencies

The build is offline after checkout; these sources are vendored with their licenses.

| Dependency | Pinned version | Source / license |
| --- | --- | --- |
| cgltf | v1.15 | https://github.com/jkuhlmann/cgltf/tree/v1.15 — MIT (`cgltf/LICENSE`) |
| MikkTSpace | 3e895b49d05ea07e4c2133156cfa94369e19e409 | https://github.com/mmikk/MikkTSpace — zlib license in source headers |
| Dear ImGui | v1.92.9b | https://github.com/ocornut/imgui/tree/v1.92.9b — MIT (`imgui/LICENSE.txt`) |
| nlohmann/json | v3.12.0 | https://github.com/nlohmann/json/tree/v3.12.0 — MIT (`nlohmann/LICENSE.MIT`) |

Only Win32 / DX12 ImGui backends are built. The DX12 backend has a small local integration patch: ignored HRESULTs and debug-only HRESULT assertions are checked in Release too; fence event/wait failures are reported. All other dependency sources are unmodified. Header hashes below refer to the unchanged upstream headers.
SHA256: cgltf.h `E378A21C084BF1F288BB799DE827BB26906EFB024255F1ECF1705EA13F11C6EC`;
mikktspace.c `DE87E74107DF766CE68108801262BD8D53899414236B59810509A8FC2A51E288`;
imgui.h `0D8DB1045DB01D908853ADFD26AE07C5BC5AB4789D4515F6EA34234A69ADE0CA`.

ScenePackage uses the unmodified single header `nlohmann/json.hpp`, SHA256
`AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63`.
It parses JSON; our small validator implements the keyword profile used by `schemas/scene-package.schema.json`.


