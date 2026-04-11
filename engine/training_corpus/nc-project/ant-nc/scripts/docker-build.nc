// SwarmOps Docker Build — prepares and builds the Docker image
// Run: NC_ALLOW_EXEC=1 nc run docker-build.nc -b build_docker

to build_docker:
    show ""
    show "SwarmOps Docker Build"
    show "====================="
    show ""

    // Copy NC source for Docker build context
    show "[1/4] Copying NC source..."
    shell("rm -rf nc-src && mkdir -p nc-src")
    shell("cp -r '../notation as code/nc/src' nc-src/src")
    shell("cp -r '../notation as code/nc/include' nc-src/include")
    shell("cp '../notation as code/nc/Makefile' nc-src/Makefile")
    show "  Done"

    // Build Docker image
    show "[2/4] Building Docker image..."
    set result to shell("docker build -t swarmops . 2>&1 | tail -5")
    show str(result)

    // Clean up
    show "[3/4] Cleaning up..."
    shell("rm -rf nc-src")
    show "  Done"

    show ""
    show "[4/4] Build complete!"
    show ""
    show "Run SwarmOps:"
    show "  docker run -p 9090:9090 -e NC_AI_KEY=sk-xxx swarmops"
    show ""
    show "Full stack (SwarmOps + Prometheus + Grafana):"
    show "  NC_AI_KEY=sk-xxx docker compose up"
    show ""
    show "Endpoints:"
    show "  SwarmOps:   http://localhost:9090"
    show "  Prometheus: http://localhost:9091"
    show "  Grafana:    http://localhost:3000 (admin/swarmops)"
    show "  Sample API: http://localhost:8080"
    show ""
