# NC Engine — Critical VM Fixes for AI Workloads

These 3 fixes in the C engine will eliminate the workarounds needed in NC-AI code and unblock deeper AI training/inference workloads.

## Fix 1: String substring support in OP_IN (`contains`)

**File:** `nc-lang/engine/src/nc_vm.c` line 486

**Problem:** `OP_IN` only handles list membership. When NC code writes `q contains "word"`, it compiles to `OP_IN` which checks if a string exists in a list — not if a substring exists in a string.

**Current code:**
```c
case OP_IN: {
    NcValue container = vm_pop(vm), needle = vm_pop(vm);
    bool found = false;
    if (IS_LIST(container) && IS_STRING(needle)) {
        // ... list membership only
    }
    vm_push(vm, NC_BOOL(found));
    break;
}
```

**Fix — add string-in-string check:**
```c
case OP_IN: {
    NcValue container = vm_pop(vm), needle = vm_pop(vm);
    bool found = false;
    if (IS_LIST(container) && IS_STRING(needle)) {
        NcList *list = AS_LIST(container);
        for (int i = 0; i < list->count; i++) {
            if (IS_STRING(list->items[i]) &&
                nc_string_equal(AS_STRING(needle), AS_STRING(list->items[i]))) {
                found = true; break;
            }
        }
    } else if (IS_STRING(container) && IS_STRING(needle)) {
        // String substring search — like Python's "in" operator
        found = strstr(AS_STRING(container)->chars,
                       AS_STRING(needle)->chars) != NULL;
    }
    vm_push(vm, NC_BOOL(found));
    break;
}
```

**Impact:** `q contains "word"` will return `true`/`false` correctly. All classification, pattern detection, and routing in NC-AI will work without the `replace()+len()` workaround.

**How Python does it:** `"word" in string` compiles to `str.__contains__()` which uses `strstr` internally — same as what we're adding.

---

## Fix 2: Local scope for function calls (not globals)

**File:** `nc-lang/engine/src/nc_vm.c` line 1530

**Problem:** Function arguments are stored in `vm->globals`. When function B is called from A, B's variables overwrite A's globals. When B returns via `respond with`, A's loop counters and variables may be corrupted.

**Current code:**
```c
// OP_CALL — stores args in GLOBALS (shared across all functions)
for (int a = 0; a < argc && a < target->var_count; a++) {
    NcValue arg = vm->stack[vm->stack_top - argc + a];
    nc_map_set(vm->globals, target->var_names[a], arg);  // BUG: globals!
}
```

**Fix — use the frame's existing locals array:**
```c
// OP_CALL — store args in the NEW frame's locals (isolated scope)
// Note: NcCallFrame already has locals[NC_LOCALS_MAX] — it's unused!

// 1. Save caller's globals that will be overwritten
NcMap *saved_locals = nc_map_new();
for (int a = 0; a < target->var_count; a++) {
    NcValue existing = nc_map_get(vm->globals, target->var_names[a]);
    if (!IS_NONE(existing)) {
        nc_map_set(saved_locals, target->var_names[a], existing);
    }
}

// 2. Set args
for (int a = 0; a < argc && a < target->var_count; a++) {
    NcValue arg = vm->stack[vm->stack_top - argc + a];
    nc_map_set(vm->globals, target->var_names[a], arg);
}
vm->stack_top -= (argc + 1);

// 3. Execute callee
NcValue call_result = nc_vm_execute(vm, target);

// 4. Restore caller's variables
for (int a = 0; a < target->var_count; a++) {
    NcValue saved = nc_map_get(saved_locals, target->var_names[a]);
    if (!IS_NONE(saved)) {
        nc_map_set(vm->globals, target->var_names[a], saved);
    } else {
        nc_map_delete(vm->globals, target->var_names[a]);
    }
}
nc_map_free(saved_locals);

vm_push(vm, call_result);
```

**Better long-term fix:** Use the existing `frame->locals[]` array instead of `vm->globals` for all variable access within a behavior. This requires updating `OP_SET_VAR` and `OP_GET_VAR` to check locals first, then globals.

**Impact:** Functions can call other functions without corrupting each other's state. Deep call chains work correctly in loops. No more "flatten everything" workarounds.

**How Python does it:** Each function call creates a new `PyFrameObject` with its own `f_locals` dict. Variables are looked up as: locals → enclosing → globals → builtins (LEGB rule).

---

## Fix 3: Configure variables accessible in behaviors

**File:** `nc-lang/engine/src/nc_compiler.c` (configure block parsing)

**Problem:** `configure: port is 9000` sets a value during service init, but it's not accessible as a variable inside `to` behaviors.

**Fix:** Store configure values in `vm->globals` during initialization, before any behavior executes. They should be treated as constants/globals.

```c
// During configure block compilation or VM init:
// For each "key is value" in configure:
nc_map_set(vm->globals, key_string, value);
```

**Impact:** Behaviors can read `port`, `max_connections`, etc. directly without hardcoding.

---

## Priority

| Fix | Effort | Impact |
|-----|--------|--------|
| Fix 1 (contains) | 5 lines of C | Unblocks ALL classification/routing in NC-AI |
| Fix 2 (local scope) | 20-30 lines of C | Unblocks deep function calls, loop reliability |
| Fix 3 (configure) | 5-10 lines of C | Convenience — configure values accessible |

**Fix 1 alone would eliminate 80% of the workarounds** in reason.nc, autonomous.nc, and codegen.nc.
