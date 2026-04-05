# text — NC String Type

## Description
The `text` type holds a sequence of characters. All text in NC is UTF-8 encoded, reference-counted, and immutable.

## Creating text
```
set name to "Hello, World!"
set empty to ""
set multi to "Line one" + " and line two"
```

## Operations
| Operation | Example | Result |
|-----------|---------|--------|
| Concatenation | `"hello" + " world"` | `"hello world"` |
| Length | `len("hello")` | `5` |
| Template | `"Hi, {{name}}!"` | `"Hi, NC!"` |
| Comparison | `name is equal to "NC"` | `true/false` |
| To string | `str(42)` | `"42"` |

## Internal
- Implemented in C as `NcString` (see `nc/src/nc_value.c`)
- FNV-1a hash for fast comparison
- Reference counted — freed when no longer used
