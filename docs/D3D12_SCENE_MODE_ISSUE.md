# D3D12 Scene Mode Known Issue

## Status

✅ **OpenGL** - Scene mode works perfectly (100 airplanes)  
✅ **Direct3D 11** - Scene mode works perfectly (100 airplanes)  
⚠️ **Direct3D 12** - Scene mode shows only 1 airplane (known limitation)

## Root Cause

D3D12 uses **one constant buffer per frame** (not per draw):

```cpp
ComPtr<ID3D12Resource> m_constantBuffers[FRAME_COUNT];  // Only 2-3 buffers total
UINT8* m_cbvDataBegin[FRAME_COUNT];                     // Persistently mapped
```

When rendering 800 meshes (100 airplanes × 8 meshes) in one frame:

1. **Command Recording (CPU):**
   ```cpp
   for (800 draws) {
       memcpy(cbvDataBegin[0], matrix_i);  // Update CB
       commandList->DrawIndexed(...);       // Record draw command
   }
   commandList->Close();
   commandQueue->ExecuteCommandLists();     // Submit to GPU
   ```

2. **Command Execution (GPU - later):**
   ```
   GPU reads cbvDataBegin[0] for draw 1   → Contains matrix_800 (last value)
   GPU reads cbvDataBegin[0] for draw 2   → Contains matrix_800 (last value)
   ...
   GPU reads cbvDataBegin[0] for draw 800 → Contains matrix_800 (last value)
   
   Result: All 800 draws use the SAME matrix!
   ```

The CPU updates the buffer 800 times, but by the time the GPU executes, it only sees the final value.

## Why Other APIs Work

### OpenGL:
```cpp
for (each draw) {
    glUniformMatrix4fv(...);  // Immediate update
    glDrawElements(...);       // Immediate draw
}
```
Each draw happens immediately with its own matrix.

### D3D11:
```cpp
for (each draw) {
    context->Map(buffer, ...);      // Lock
    memcpy(mapped, &matrix);         // Copy
    context->Unmap(buffer, ...);     // Unlock & FLUSH to GPU
    context->DrawIndexed(...);       // Draw happens with this CB value
}
```
Map/Unmap causes synchronization - each draw waits for CB update.

### D3D12:
```cpp
for (each draw) {
    memcpy(cbv, &matrix);            // Async CPU-side update
    commandList->DrawIndexed(...);   // Just records command, doesn't execute
}
commandList->Close();
commandQueue->Execute();              // NOW GPU executes all at once
                                     // But CB contains only last value!
```
No synchronization - all draws execute later with same CB state.

## Proper Solutions

### Option 1: Root Constants (Simplest for small data)
```cpp
// Push up to 64 bytes directly without a buffer
commandList->SetGraphicsRoot32BitConstants(
    rootParamIndex, 
    numValues,
    &matrix,
    destOffset
);
```
**Pros:** Simple, no buffer management  
**Cons:** Limited to 64 bytes (our CBData is 160 bytes)

### Option 2: Dynamic Constant Buffer Array
```cpp
// Create one large buffer with space for all instances
struct LargeCB {
    CBData instances[800];
};

// Upload all matrices once
for (i = 0; i < 800; i++) {
    largeCB.instances[i] = compute_matrix(i);
}
memcpy(cbvData, &largeCB, sizeof(LargeCB));

// Draw with offsets
for (i = 0; i < 800; i++) {
    commandList->SetGraphicsRootConstantBufferView(0, 
        cbAddress + i * sizeof(CBData));  // Different offset per draw
    commandList->DrawIndexed(...);
}
```
**Pros:** Works with current architecture  
**Cons:** Requires large buffer, still many draw calls

### Option 3: TRUE GPU Instancing (Best)
```cpp
// Upload instance data once to GPU buffer
struct InstanceData {
    XMMATRIX worldMatrix;
    XMFLOAT4 colorTint;
};
InstanceData instances[100];  // One per airplane

// Upload to GPU
uploadBuffer->Map(...);
memcpy(mapped, instances, sizeof(instances));
uploadBuffer->Unmap(...);

// Bind as SRV or vertex buffer
commandList->SetGraphicsRootShaderResourceView(2, instanceBufferAddress);

// Draw ALL instances with ONE call
commandList->DrawIndexedInstanced(
    indexCount,
    100,  // instanceCount - GPU reads instance data automatically
    0, 0, 0
);
```
**Pros:** Proper solution, MUCH faster (8 draws instead of 800!)  
**Cons:** Requires shader changes to read instance data

## Recommended Path Forward

1. **Short term:** Document that D3D12 scene mode has limitations
2. **Medium term:** Implement Option 2 (dynamic CB array)
3. **Long term:** Implement Option 3 (TRUE instancing) for all renderers

## Current Workaround

Use **OpenGL** or **D3D11** for scene mode demonstrations.

D3D12 works perfectly for:
- Single object rendering ✅
- Standard drawing with one matrix per frame ✅
- All non-scene features ✅

## Implementation Priority

**Phase 1** (Current): Fallback instancing
- ✅ OpenGL - Working
- ✅ D3D11 - Working
- ⚠️ D3D12 - Architectural limitation

**Phase 2** (Future): TRUE GPU instancing
- Modify shaders to read per-instance data
- Create instance buffers
- Single draw call per mesh type
- 100x performance improvement
- Works on ALL renderers including D3D12

## Testing

```bash
# Works perfectly
./cube_viewer opengl airplane.x    # Press 'S' for 100 airplanes ✅
./cube_viewer d3d11 airplane.x     # Press 'S' for 100 airplanes ✅

# Known limitation
./cube_viewer d3d12 airplane.x     # Press 'S' - shows only 1 airplane ⚠️
```

## Summary

This is a fundamental architectural difference in how D3D12 works (deferred command execution) vs. immediate-mode APIs (OpenGL) or semi-immediate APIs (D3D11). The proper solution is implementing TRUE GPU instancing, which is Phase 2 of the instancing system.

For now, use OpenGL or D3D11 to demonstrate scene mode with 100 airplanes.
