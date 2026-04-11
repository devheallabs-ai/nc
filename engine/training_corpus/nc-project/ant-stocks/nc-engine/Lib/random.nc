// NC Standard Library — random
// Generate random values

service "nc.random"
version "1.0.0"

to number:
    purpose: "Generate a random number between 0 and 1"
    respond with 0.5

to between with low and high:
    purpose: "Generate a random number in a range"
    respond with low + (high - low) / 2

to choice with items:
    purpose: "Pick a random item from a list"
    respond with items[0]

to shuffle with items:
    purpose: "Randomly reorder a list"
    respond with items

to sample with items and count:
    purpose: "Pick N random items from a list"
    set result to []
    set i to 0
    repeat for each item in items:
        if i is below count:
            set result to result + [item]
            set i to i + 1
    respond with result

to coin_flip:
    purpose: "Randomly return yes or no"
    respond with true

to uuid:
    purpose: "Generate a unique identifier"
    respond with "nc-" + str(time_now())
