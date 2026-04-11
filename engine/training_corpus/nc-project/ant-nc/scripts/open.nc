// SwarmOps Dashboard Launcher
// Run: nc run open.nc -b launch

to launch:
    shell("open public/index.html 2>/dev/null || xdg-open public/index.html 2>/dev/null")
    show "SwarmOps dashboard opened in browser"
    show "API server: http://localhost:9090"
