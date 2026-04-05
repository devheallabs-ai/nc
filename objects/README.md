# NC Built-in Objects

This directory documents the built-in types that the NC engine provides.
These are implemented in C (in `nc/src/nc_value.c`) and available in every NC program.

## Types

| NC Type | Description | C Implementation |
|---------|-------------|-----------------|
| text | UTF-8 string | `NcString` (refcounted, hashed) |
| number | integer (64-bit) or float (64-bit) | `int64_t` / `double` |
| yesno | boolean | `bool` |
| list | ordered, dynamic array | `NcList` (growable) |
| record | key-value map | `NcMap` (linear probe) |
| nothing | null/absent value | `VAL_NONE` |

## Files

- `textobject.md` — text (string) type documentation
- `numberobject.md` — number (int/float) type documentation
- `listobject.md` — list type documentation
- `recordobject.md` — record (map) type documentation
- `boolobject.md` — yesno (boolean) type documentation
