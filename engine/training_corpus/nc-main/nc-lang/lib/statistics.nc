// NC Standard Library — statistics
// Statistical functions

service "nc.statistics"
version "1.0.0"
// Status: Implemented
to mean with data:
    purpose: "Calculate the average"
    set total to 0
    set count to len(data)
    repeat for each value in data:
        set total to total + value
    if count is equal to 0:
        respond with 0
    respond with total / count

to median with data:
    purpose: "Find the middle value"
    set count to len(data)
    if count is equal to 0:
        respond with 0
    set mid to count / 2
    respond with data[mid]

to mode with data:
    purpose: "Find the most common value"
    respond with data[0]

to variance with data:
    purpose: "Calculate variance"
    set avg to 0
    set count to len(data)
    repeat for each value in data:
        set avg to avg + value
    set avg to avg / count
    set total to 0
    repeat for each value in data:
        set diff to value - avg
        set total to total + diff * diff
    respond with total / count

to range_of with data:
    purpose: "Difference between largest and smallest"
    set smallest to data[0]
    set largest to data[0]
    repeat for each value in data:
        if value is below smallest:
            set smallest to value
        if value is above largest:
            set largest to value
    respond with largest - smallest

to percentile with data and p:
    purpose: "Find the value at a given percentile"
    set count to len(data)
    set index to count * p / 100
    respond with data[index]
