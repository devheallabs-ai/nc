service "test-memory"
version "1.0.0"

to test_memory_create:
    set mem to memory_new(10)
    respond with mem

to test_memory_add_and_get:
    set mem to memory_new(10)
    memory_add(mem, "user", "hello")
    memory_add(mem, "assistant", "hi there")
    set msgs to memory_get(mem)
    if len(msgs) is equal 2:
        respond with "pass"
