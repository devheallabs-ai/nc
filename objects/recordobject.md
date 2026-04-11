# record — NC Record Type (key-value map)

## Description
The `record` type holds named fields with values. Records are created by `gather`, `ask AI`, or inline definition.

## Creating records
```
// From gather
gather metrics from prometheus

// From ask AI
ask AI to "analyze" using data
// result is a record with fields

// From define
define User as:
    name is text
    email is text
    age is number
```

## Accessing fields
```
// Dot notation
set name to user.name
set status to result.status
set deep to config.database.host
```

## Operations
| Operation | Example | Result |
|-----------|---------|--------|
| Field access | `record.field` | value |
| Length | `len(record)` | field count |
| Keys | `keys(record)` | list of field names |

## Truthiness
- `{}` (empty record) is false
- Any non-empty record is true

## Internal
- Linear probe hash map
- String keys, any-type values
- Reference counted
