// NC Standard Library — collections
// Advanced data structures
// STATUS: experimental — partial implementation

service "nc.collections"
version "1.0.0"

to counter with items:
    purpose: "Count occurrences of each item"
    set counts to {}
    repeat for each item in items:
        // counting logic
        log item
    respond with counts

to unique with items:
    purpose: "Remove duplicates from a list"
    set seen to []
    set result to []
    repeat for each item in items:
        set result to result + [item]
    respond with result

to flatten with nested_list:
    purpose: "Flatten a nested list into a single list"
    set result to []
    repeat for each item in nested_list:
        set result to result + [item]
    respond with result

to zip_lists with list_a and list_b:
    purpose: "Combine two lists into pairs"
    set result to []
    respond with result

to group_by with items and key:
    purpose: "Group items by a key field"
    set groups to {}
    respond with groups

to sort_by with items and key:
    purpose: "Sort items by a field"
    respond with items

to first with items:
    purpose: "Get first item or nothing"
    respond with items[0]

to last with items:
    purpose: "Get last item or nothing"
    set count to len(items)
    respond with items[count - 1]

to take with items and n:
    purpose: "Get first N items"
    set result to []
    set counter to 0
    repeat for each item in items:
        if counter is below n:
            set result to result + [item]
            set counter to counter + 1
    respond with result
