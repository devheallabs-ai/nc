service "WeatherBot"

set api_key to env("WEATHER_API_KEY")
set base_url to "https://api.openweathermap.org/data/2.5/weather"

to fetch_weather with params:
    set city to params["city"]
    set url to base_url + "?q=" + city + "&appid=" + api_key + "&units=metric"
    log "Fetching weather for: " + city
    try:
        gather data from url save as raw_response
        set weather to json_decode(raw_response)
        respond with weather
    on error:
        log "Failed to fetch weather for " + city
        respond with {"error": "Could not fetch weather data for " + city}

to format_weather with weather:
    set report to {}
    set report["city"] to weather["name"]
    set report["country"] to weather["sys"]["country"]
    set report["temperature"] to weather["main"]["temp"]
    set report["feels_like"] to weather["main"]["feels_like"]
    set report["humidity"] to weather["main"]["humidity"]
    set report["wind_speed"] to weather["wind"]["speed"]
    set report["condition"] to weather["weather"][0]["main"]
    set report["description"] to weather["weather"][0]["description"]
    respond with report

to get_weather with params:
    set city to params["city"] or "London"
    set weather to fetch_weather({"city": city})
    if weather["error"]:
        respond with weather
    set report to format_weather(weather)
    respond with report

to get_advice with params:
    set city to params["city"] or "London"
    set weather to fetch_weather({"city": city})
    if weather["error"]:
        respond with weather
    set report to format_weather(weather)
    set weather_text to "City: " + report["city"] + "\nTemperature: " + report["temperature"] + "C\nFeels like: " + report["feels_like"] + "C\nHumidity: " + report["humidity"] + "%\nWind: " + report["wind_speed"] + " m/s\nCondition: " + report["description"]
    ask AI to "Based on this weather data, give practical advice on what to wear, whether to carry an umbrella, and any health precautions. Be concise and friendly:\n" + weather_text using weather_text save as advice
    set result to {}
    set result["weather"] to report
    set result["advice"] to advice
    respond with result

to suggest_activities with params:
    set city to params["city"] or "London"
    set weather to fetch_weather({"city": city})
    if weather["error"]:
        respond with weather
    set report to format_weather(weather)
    set weather_text to "City: " + report["city"] + "\nTemp: " + report["temperature"] + "C\nCondition: " + report["description"] + "\nWind: " + report["wind_speed"] + " m/s\nHumidity: " + report["humidity"] + "%"
    ask AI to "Suggest 5 activities that would be great for this weather. Include a mix of indoor and outdoor options. For each activity, briefly explain why it suits the current conditions:\n" + weather_text using weather_text save as activities
    set result to {}
    set result["weather"] to report
    set result["activities"] to activities
    respond with result

to natural_query with body:
    set question to body["question"]
    set city to body["city"] or "London"
    log "Processing natural language query: " + question
    set weather to fetch_weather({"city": city})
    if weather["error"]:
        respond with weather
    set report to format_weather(weather)
    set weather_text to "City: " + report["city"] + "\nTemperature: " + report["temperature"] + "C\nFeels like: " + report["feels_like"] + "C\nHumidity: " + report["humidity"] + "%\nWind: " + report["wind_speed"] + " m/s\nCondition: " + report["description"]
    set prompt to "You are a helpful weather assistant. Answer this question using the weather data provided. Be conversational and helpful.\n\nQuestion: " + question + "\n\nWeather Data:\n" + weather_text
    ask AI to prompt using weather_text save as answer
    set result to {}
    set result["question"] to question
    set result["answer"] to answer
    set result["weather"] to report
    respond with result

to compare_weather with body:
    set cities to body["cities"]
    set reports to []
    repeat for each city in cities:
        set weather to fetch_weather({"city": city})
        if weather["error"]:
            append {"city": city, "error": "Data unavailable"} to reports
        otherwise:
            set report to format_weather(weather)
            append report to reports
    set comparison_text to ""
    repeat for each report in reports:
        if report["error"]:
            set comparison_text to comparison_text + report["city"] + ": Data unavailable\n"
        otherwise:
            set comparison_text to comparison_text + report["city"] + ": " + report["temperature"] + "C, " + report["description"] + ", humidity " + report["humidity"] + "%\n"
    ask AI to "Compare the weather across these cities. Which has the best weather right now and why? Be brief:\n" + comparison_text using comparison_text save as comparison
    set result to {}
    set result["cities"] to reports
    set result["comparison"] to comparison
    respond with result

api:
    GET /weather/:city runs get_weather
    GET /advice/:city runs get_advice
    GET /activities/:city runs suggest_activities
    POST /ask runs natural_query
    POST /compare runs compare_weather
