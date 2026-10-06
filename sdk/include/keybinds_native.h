#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ZmlKeyActionV1 {
    uint32_t size;
    const char *owner, *id, *name, *primary, *secondary;
} ZmlKeyActionV1;
typedef struct ZmlKeybindsV1 {
    uint32_t size, abi;
    uint64_t (*register_action)(const ZmlKeyActionV1* action);
    void (*unregister_action)(uint64_t handle);
    // Thread-safe; library owns physical input, focus gate, saved keys and edges.
    // One polling consumer per handle. Returns 0 when disabled/invalid/editing.
    int (*down)(uint64_t handle);
    int (*pressed)(uint64_t handle);
} ZmlKeybindsV1;
typedef const ZmlKeybindsV1* (*ZmlKeybindsEntry)(void);
// Keybinds.dll export ZML_KeybindsV1. Inputs copied; no cross-DLL allocation.
#ifdef __cplusplus
}
#endif
