// Test: Type definitions
// Verifies define/as type declarations

service "test-types"
version "1.0.0"

define User as:
    name is text
    age is number
    email is text optional

define Config as:
    host is text
    port is number
    debug is yesno

to test types exist:
    respond with "types defined"
