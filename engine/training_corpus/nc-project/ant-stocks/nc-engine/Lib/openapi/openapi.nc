module "openapi"

purpose: "Generate OpenAPI 3.1 specifications from NC service definitions"

to generate_spec with service_name, version, routes, behaviors:
    purpose: "Produce an OpenAPI JSON specification"

    set spec to {
        "openapi": "3.1.0",
        "info": {
            "title": service_name,
            "version": version,
            "description": "Auto-generated from NC service definition"
        },
        "servers": [
            {"url": "http://localhost:8080", "description": "Local development"}
        ],
        "paths": {},
        "components": {
            "securitySchemes": {
                "bearerAuth": {
                    "type": "http",
                    "scheme": "bearer",
                    "bearerFormat": "JWT"
                },
                "apiKeyAuth": {
                    "type": "apiKey",
                    "in": "header",
                    "name": "X-Api-Key"
                }
            }
        }
    }

    respond with spec
