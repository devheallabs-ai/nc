// Test: import keyword — module loading
// Verifies import statement parses and modules resolve

service "test-import"
version "1.0.0"

import "math"

to test import exists:
    set x to 42
    respond with x

to test after import:
    set result to abs(-10)
    respond with result
