<p align="center">
  <img src="docs/assets/nc_mascot.png" alt="NC Mascot" width="250">
</p>

# Learn NC in 10 Minutes

NC is a programming language written in plain English.
If you can write instructions for a person, you can write NC.

## Lesson 1: Your First Program

Create a file called `hello.nc`:
```
service "hello"
version "1.0.0"

to greet:
    respond with "Hello, World!"
```

Run it:
```bash
nc run hello.nc -b greet
```

Output:
```
  Result: Hello, World!
```

**What happened:** You defined a service with one behavior (function) called `greet` that returns a string.

## Lesson 2: Variables

```
to calculate:
    set price to 100
    set tax to price * 0.1
    set total to price + tax
    respond with total
```

Variables are created with `set X to VALUE`. No type declarations needed.

## Lesson 3: Parameters

```
to greet with name:
    respond with "Hello, " + name + "!"
```

Parameters come after `with`. Multiple parameters use `and`:

```
to add with a and b:
    respond with a + b
```

## Lesson 4: Conditions

NC uses plain English for comparisons:

```
to check_score with score:
    if score is above 90:
        respond with "excellent"
    otherwise:
        respond with "keep trying"
```

Available comparisons:
- `is above` (greater than)
- `is below` (less than)
- `is equal to`
- `is not equal to`
- `is at least` (greater than or equal)
- `is at most` (less than or equal)

## Lesson 5: Loops

```
to count_items with items:
    set total to 0
    repeat for each item in items:
        set total to total + item
    respond with total
```

## Lesson 6: Pattern Matching

```
to classify with status:
    match status:
        when "healthy":
            respond with "All good"
        when "degraded":
            respond with "Watch closely"
        when "critical":
            respond with "Fix now!"
        otherwise:
            respond with "Unknown status"
```

## Lesson 7: AI Operations

This is what makes NC special — AI is built into the language:

```
to analyze with data:
    ask AI to "classify this data and find patterns" using data:
        confidence: 0.8
        save as: analysis
    respond with analysis
```

## Lesson 8: Gathering Data

Pull data from external systems:

```
to check_health:
    gather metrics from prometheus:
        query: "up{job='my-service'}"
        range: "1h"
    gather logs from loki:
        query: "{app='my-service'}"
    respond with metrics
```

## Lesson 9: Types

Define your own data types:

```
define User as:
    name is text
    email is text
    age is number
    active is yesno
    role is text optional
```

Types: `text`, `number`, `yesno`, `list`, `record`, `optional`.

## Lesson 10: Building a Service

Put it all together:

```
service "task-manager"
version "1.0.0"
model "nova"

define Task as:
    title is text
    priority is text
    done is yesno

to create task with title and priority:
    purpose: "Create a new task"
    set task to {"title": title, "priority": priority, "done": false}
    store task into "tasks"
    respond with task

to list tasks:
    purpose: "Show all tasks"
    gather tasks from database
    respond with tasks

to smart prioritize with tasks:
    purpose: "Use AI to prioritize tasks"
    ask AI to "prioritize these tasks by urgency and importance" using tasks:
        save as: prioritized
    respond with prioritized

api:
    POST /tasks runs create_task
    GET /tasks runs list_tasks
    POST /tasks/prioritize runs smart_prioritize
    GET /health runs health_check
```

Run it:
```bash
nc run task_manager.nc
```

## What's Next?

- Read the [Language Spec](docs/LANGUAGE_SPEC.md) for complete syntax reference
- Look at [Examples](docs/EXAMPLES.md) for real-world programs
- Check the [FAQ](docs/FAQ.md) for common questions
- Try `nc repl` for interactive experimentation
- Try `nc digest your_python_file.py` to convert existing code
