# list — NC List Type

## Description
The `list` type holds an ordered collection of values. Lists can contain any mix of types.

## Creating lists
```
set numbers to [1, 2, 3, 4, 5]
set mixed to ["hello", 42, true]
set empty to []
```

## Operations
| Operation | Example | Result |
|-----------|---------|--------|
| Length | `len([1, 2, 3])` | `3` |
| Index | `items[0]` | first item |
| Iterate | `repeat for each item in items:` | loops |

## Iteration
```
set total to 0
repeat for each item in [10, 20, 30]:
    set total to total + item
// total is 60
```

## Truthiness
- `[]` (empty list) is false
- Any non-empty list is true

## Internal
- Dynamic array with automatic growth
- Reference counted
- O(1) index access, O(1) amortized append
