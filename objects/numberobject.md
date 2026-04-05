# number — NC Numeric Type

## Description
The `number` type represents integers (whole numbers) and floating-point numbers (decimals). NC automatically handles the distinction.

## Creating numbers
```
set count to 42
set price to 19.99
set negative to -5
set zero to 0
```

## Operations
| Operation | Example | Result |
|-----------|---------|--------|
| Addition | `10 + 3` | `13` |
| Subtraction | `10 - 3` | `7` |
| Multiplication | `10 * 3` | `30` |
| Division | `10 / 3` | `3.333...` |
| Comparison | `10 is above 5` | `true` |
| At least | `10 is at least 10` | `true` |
| At most | `10 is at most 20` | `true` |

## Truthiness
- `0` is false
- Any other number is true

## Internal
- Integers: 64-bit signed (`int64_t`)
- Floats: 64-bit double precision (`double`)
- Integer arithmetic stays integer when possible
- Division always produces float
