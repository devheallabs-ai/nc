# ═══════════════════════════════════════════════════════════
#  NC (Notation-as-Code) — Official Docker Image
#
#  Multi-platform: linux/amd64, linux/arm64
#
#  Variants (like Python):
#    nc:latest     — Alpine-based, minimal (~20MB)
#    nc:slim       — Debian slim (~80MB)  [see Dockerfile.slim]
#
#  Usage:
#    FROM nc:latest
#    COPY service.nc /app/
#    CMD ["nc", "serve", "/app/service.nc"]
#
#  Build:
#    docker build -t nc .
#    docker buildx build --platform linux/amd64,linux/arm64 -t nc .
#
#  Run:
#    docker run -it nc version
#    docker run -v $(pwd):/app nc run /app/service.nc
#    docker run -p 8080:8080 -v $(pwd):/app nc serve /app/service.nc
# ═══════════════════════════════════════════════════════════

# ── Stage 1: Build ──────────────────────────────────────────
FROM alpine:3.21 AS builder

RUN echo "http://dl-cdn.alpinelinux.org/alpine/v3.21/main"      > /etc/apk/repositories && \
    echo "http://dl-cdn.alpinelinux.org/alpine/v3.21/community" >> /etc/apk/repositories && \
    apk add --no-cache gcc musl-dev make curl-dev libucontext-dev

WORKDIR /build/nc
COPY engine/src/       src/
COPY engine/include/   include/
COPY engine/Makefile   Makefile

RUN make clean && make LDFLAGS="-lm -lcurl -lpthread -ldl -lucontext" CFLAGS="-Wall -Wextra -O2 -std=c11 -I include -Wno-unused-function -Wno-unused-variable -fomit-frame-pointer -DNDEBUG -D_GNU_SOURCE -DNC_NO_REPL -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1"

# ── Stage 2: Runtime ────────────────────────────────────────
FROM alpine:3.21

RUN echo "http://dl-cdn.alpinelinux.org/alpine/v3.21/main"      > /etc/apk/repositories && \
    echo "http://dl-cdn.alpinelinux.org/alpine/v3.21/community" >> /etc/apk/repositories && \
    apk add --no-cache libcurl ca-certificates libucontext

RUN addgroup -S nc && adduser -S nc -G nc

COPY --from=builder /build/nc/build/nc /usr/local/bin/nc

RUN mkdir -p /app /nc/lib /var/log/nc && \
    chown -R nc:nc /app /var/log/nc

COPY lib/ /nc/lib/

ENV NC_LIB_PATH=/nc/lib
ENV NC_LOG_FORMAT=json
ENV NC_AUDIT_FORMAT=json
ENV NC_AUDIT_FILE=/var/log/nc/audit.jsonl

LABEL org.opencontainers.image.title="NC" \
      org.opencontainers.image.description="Notation-as-Code: The AI Programming Language" \
      org.opencontainers.image.vendor="DevHeal Labs AI" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.source="https://github.com/devheallabs-ai/nc"

WORKDIR /app

USER nc

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD nc version || exit 1

EXPOSE 8080

ENTRYPOINT ["nc"]
CMD ["version"]
