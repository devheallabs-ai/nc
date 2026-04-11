// SwarmOps Demo — Run: NC_ALLOW_EXEC=1 nc run demo.nc -b demo

to demo:
    show ""
    show "SwarmOps v6.0 — Incident Copilot Demo"
    show ""
    show "[1/4] Health check..."
    gather health from "http://localhost:9090/health"
    show health
    show ""
    show "[2/4] Config check..."
    gather config from "http://localhost:9090/config"
    show config
    show ""
    show "[3/4] Sample incident data..."
    gather sample from "http://localhost:9090/sample"
    show sample
    show ""
    show "[4/4] Listing incidents..."
    gather incidents from "http://localhost:9090/incidents"
    show incidents
    show ""
    show "Demo complete. To investigate, add your AI key to .env and run:"
    show "  nc post http://localhost:9090/investigate '{\"service_name\":\"checkout-api\",\"description\":\"HTTP 500 errors\"}'"
