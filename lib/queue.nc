// NC Standard Library — queue
// First-in-first-out and priority queues

service "nc.queue"
version "1.0.0"
// Status: Implemented
to create:
    purpose: "Create an empty queue"
    respond with []

to push with queue and item:
    purpose: "Add an item to the back of the queue"
    respond with queue + [item]

to pop with queue:
    purpose: "Remove and return the front item"
    if len(queue) is equal to 0:
        respond with nothing
    respond with queue[0]

to peek with queue:
    purpose: "Look at the front item without removing it"
    if len(queue) is equal to 0:
        respond with nothing
    respond with queue[0]

to is_empty with queue:
    purpose: "Check if the queue has no items"
    if len(queue) is equal to 0:
        respond with true
    respond with false

to size with queue:
    purpose: "Get the number of items in the queue"
    respond with len(queue)
