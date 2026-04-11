service "test-slicing"
version "1.0.0"

to test_string_slice_middle:
    set text to "Hello, World!"
    set result to text[0:5]
    respond with result

to test_string_slice_from_start:
    set text to "Hello, World!"
    set result to text[:5]
    respond with result

to test_string_slice_to_end:
    set text to "Hello, World!"
    set result to text[7:]
    respond with result

to test_string_negative_index:
    set text to "Hello"
    set result to text[-2:]
    respond with result

to test_list_slice:
    set items to [10, 20, 30, 40, 50]
    set middle to items[1:4]
    respond with len(middle)

to test_list_slice_from_start:
    set items to [10, 20, 30, 40, 50]
    set first_three to items[:3]
    respond with len(first_three)

to test_list_slice_to_end:
    set items to [10, 20, 30, 40, 50]
    set last_two to items[3:]
    respond with len(last_two)

to test_list_negative_slice:
    set items to [10, 20, 30, 40, 50]
    set tail to items[-2:]
    respond with len(tail)

to test_empty_slice:
    set text to "Hello"
    set result to text[3:3]
    respond with result

to test_extract_domain:
    set email to "user@company.com"
    set parts to split(email, "@")
    respond with parts[1]
