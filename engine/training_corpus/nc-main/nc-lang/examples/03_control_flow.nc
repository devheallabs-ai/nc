// Control Flow — Shows how NC handles decisions and loops
// Plain English, no coding knowledge needed

service "control-flow-demo"
version "1.0.0"

to grade a score:
    purpose: "Turn a number into a grade"

    if score is above 90:
        respond with "excellent"
    otherwise:
        if score is above 70:
            respond with "good"
        otherwise:
            if score is above 50:
                respond with "average"
            otherwise:
                respond with "needs improvement"

to check status with status:
    purpose: "React differently based on system status"

    match status:
        when "healthy":
            respond with "System is running normally"
        when "degraded":
            respond with "Performance issues detected"
        when "critical":
            respond with "Immediate attention required"
        otherwise:
            respond with "Unknown status: " + status

to add up items with items:
    purpose: "Add up all numbers in a list"

    set total to 0
    repeat for each item in items:
        set total to total + item

    respond with total

to safely process with input:
    purpose: "Handle errors gracefully"

    try:
        if input is equal to "":
            log "Input was empty!"
            respond with "Error: empty input"
        set result to "Processed: " + input
        respond with result
    on error:
        log "Something went wrong: " + error
        respond with "Error occurred"

api:
    POST /grade runs grade
    POST /status runs check_status
    POST /sum runs add_up
    POST /process runs safely_process
