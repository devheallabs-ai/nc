<|begin|>
// Description: a recipe app with meal planning and AI ingredient substitution
// Type: full app
service "recipes"
version "1.0.0"

to list recipes:
    set recipes to load("recipes.json")
    respond with recipes

to get recipe with id:
    set recipe to load("recipes.json", id)
    respond with recipe

to create recipe with data:
    set data.id to generate_id()
    set data.created_at to now()
    set recipes to load("recipes.json")
    add data to recipes
    save recipes to "recipes.json"
    respond with data

to search recipes with query:
    set recipes to load("recipes.json")
    set results to search(recipes, query)
    respond with results

to list by ingredient with ingredient:
    set recipes to load("recipes.json")
    set matching to filter_contains(recipes, "ingredients", ingredient)
    respond with matching

api:
    GET /recipes runs list_recipes
    GET /recipes/:id runs get_recipe
    POST /recipes runs create_recipe
    GET /recipes/search runs search_recipes
    GET /recipes/by-ingredient runs list_by_ingredient
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "recipes"}

// === NC_FILE_SEPARATOR ===

page "Recipes"
title "Recipes | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

nav:
    brand "Recipes"
    input "search" placeholder "Search recipes..." action "search"

section "featured":
    heading "Featured Recipes"
    grid from "/recipes" as recipes columns 3:
        card:
            image recipes.image
            heading recipes.name
            text recipes.prep_time
            text recipes.difficulty
            link "View Recipe" to "/recipes/{{recipes.id}}"

section "recipe-detail":
    card:
        heading "Ingredients"
        list as ingredients:
            text ingredients
        heading "Instructions"
        list as steps:
            text steps
        button "Get Substitutions" action "getSubstitutions" style ai

// === NC_AGENT_SEPARATOR ===

// Recipes AI Agent
service "recipes-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to substitute ingredient with ingredient and dietary:
    ask AI to "Suggest substitutes for {{ingredient}} that are {{dietary}}-friendly" save as substitutes
    respond with {"substitutes": substitutes}

to generate recipe from ingredients with ingredients:
    ask AI to "Create a recipe using these ingredients: {{ingredients}}" save as recipe
    respond with {"recipe": recipe}

to scale recipe with recipe and servings:
    ask AI to "Scale this recipe from {{recipe.servings}} to {{servings}} servings: {{recipe.ingredients}}" save as scaled
    respond with {"ingredients": scaled}

to handle with prompt:
    purpose: "Handle user request for recipes"
    ask AI to "You are a helpful recipes assistant. {{prompt}}" save as response
    respond with {"reply": response}

to classify with input:
    ask AI to "Classify as: create, read, update, delete, help. Input: {{input}}" save as intent
    respond with {"intent": intent}

api:
    POST /agent          runs handle
    POST /agent/classify  runs classify
    GET  /agent/health    runs health_check

to health check:
    respond with {"status": "ok", "ai": "local"}
<|end|>
