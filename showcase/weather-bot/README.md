# Weather Bot

An AI-powered weather assistant built with NC that fetches live weather data and provides intelligent recommendations.

## Features

- Live weather data from OpenWeatherMap API
- AI-generated clothing and preparation advice
- Activity suggestions based on current conditions
- Natural language weather queries
- Multi-city weather comparison

## Setup

Set your OpenWeatherMap API key as an environment variable:

```
export WEATHER_API_KEY=your_key_here
```

Get a free API key at [openweathermap.org](https://openweathermap.org/api).

## How to Run

```bash
nc serve weather_bot.nc
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/weather/:city` | Get current weather for a city |
| `GET` | `/advice/:city` | Get weather + AI advice (clothing, umbrella, etc.) |
| `GET` | `/activities/:city` | Get weather + 5 AI-suggested activities |
| `POST` | `/ask` | Ask a natural language weather question |
| `POST` | `/compare` | Compare weather across multiple cities |

## Example Usage

**Get weather:**
```
GET /weather/Tokyo
```

**Get AI advice:**
```
GET /advice/London
```

Response includes weather data plus advice like:
> "It's 8C and drizzling. Wear a warm jacket and carry an umbrella. The wind is mild, so a scarf should suffice."

**Ask a question:**
```json
POST /ask
{
  "city": "Paris",
  "question": "Should I go for a run this evening?"
}
```

**Compare cities:**
```json
POST /compare
{
  "cities": ["New York", "London", "Tokyo", "Sydney"]
}
```

## curl Examples

**Get current weather:**
```bash
curl http://localhost:8080/weather/Tokyo
```

**Get AI advice:**
```bash
curl http://localhost:8080/advice/London
```

**Get activity suggestions:**
```bash
curl http://localhost:8080/activities/Paris
```

**Ask a natural language question:**
```bash
curl -X POST http://localhost:8080/ask \
  -H "Content-Type: application/json" \
  -d '{"city": "Paris", "question": "Should I go for a run this evening?"}'
```

**Compare cities:**
```bash
curl -X POST http://localhost:8080/compare \
  -H "Content-Type: application/json" \
  -d '{"cities": ["New York", "London", "Tokyo", "Sydney"]}'
```

## How It Works

1. Weather data is fetched from OpenWeatherMap using `gather data from`
2. The raw response is parsed and formatted into a clean report
3. AI interprets the data contextually using `ask AI to` prompts
4. Results combine raw data with AI-generated insights
