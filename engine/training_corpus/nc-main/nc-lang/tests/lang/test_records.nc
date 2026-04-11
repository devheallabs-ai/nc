// Test: Records (maps/objects in NC)
// Verifies creation, dot access, nested records, iteration

service "test-records"
version "1.0.0"

to test create record:
    set person to {"name": "Alice", "age": 30}
    respond with person.name

to test record age:
    set person to {"name": "Bob", "age": 25}
    respond with person.age

to test nested record:
    set data to {"user": {"name": "Charlie", "role": "admin"}}
    respond with data.user.role

to test record keys:
    set config to {"host": "localhost", "port": 8080}
    respond with len(keys(config))

to test record values:
    set scores to {"math": 90, "science": 85}
    set vals to values(scores)
    respond with len(vals)

to test record update:
    set item to {"status": "pending", "count": 0}
    set item.status to "done"
    set item.count to 5
    respond with item.status

to test record iteration:
    set total to 0
    set prices to {"apple": 1, "banana": 2, "cherry": 3}
    repeat for each key, value in prices:
        set total to total + value
    respond with total

to test empty record:
    set empty to {}
    respond with len(keys(empty))

to test record with list:
    set data to {"tags": ["nc", "ai", "plain-english"]}
    respond with len(data.tags)

to test record in list:
    set users to [{"name": "Alice"}, {"name": "Bob"}]
    respond with users[1].name
