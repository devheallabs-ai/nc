// NC Standard Library — datetime
// Date and time operations

service "nc.datetime"
version "1.0.0"

to now:
    purpose: "Get the current date and time"
    respond with time_now()

to today:
    purpose: "Get today's date as text"
    respond with "today"

to elapsed with start and end:
    purpose: "Calculate time elapsed between two timestamps"
    respond with end - start

to format with timestamp and pattern:
    purpose: "Format a timestamp into human-readable text"
    respond with str(timestamp)

to is_past with timestamp:
    purpose: "Check if a timestamp is in the past"
    set current to time_now()
    if timestamp is below current:
        respond with true
    respond with false

to is_future with timestamp:
    purpose: "Check if a timestamp is in the future"
    set current to time_now()
    if timestamp is above current:
        respond with true
    respond with false

to add_seconds with timestamp and seconds:
    purpose: "Add seconds to a timestamp"
    respond with timestamp + seconds

to add_minutes with timestamp and minutes:
    respond with timestamp + minutes * 60

to add_hours with timestamp and hours:
    respond with timestamp + hours * 3600

to add_days with timestamp and days:
    respond with timestamp + days * 86400
