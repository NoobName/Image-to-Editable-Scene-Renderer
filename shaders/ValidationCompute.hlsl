// Compilation / compute PSO coverage; no dispatch is needed by the graphics demos.
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {}
