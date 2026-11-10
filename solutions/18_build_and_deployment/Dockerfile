# syntax=docker/dockerfile:1

# Build stage: pinned toolchain, compiles the service with build metadata stamped in.
FROM gcc:14.2.0-bookworm AS build
ARG CPPREF_BUILD_VERSION=0.1.0
ARG CPPREF_BUILD_GIT_SHA=unknown
WORKDIR /src
COPY app/main.cpp app/main.cpp
COPY config.hpp config.cpp cli.hpp cli.cpp build_info.hpp build_info.cpp shutdown.hpp shutdown.cpp health.hpp health.cpp container_lint.hpp container_lint.cpp ./
RUN mkdir -p /out && \
    g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -I/src -static-libstdc++ -static-libgcc -pthread \
      -DCPPREF_BUILD_VERSION=\"${CPPREF_BUILD_VERSION}\" \
      -DCPPREF_BUILD_GIT_SHA=\"${CPPREF_BUILD_GIT_SHA}\" \
      -o /out/cppref-service app/main.cpp config.cpp cli.cpp build_info.cpp shutdown.cpp health.cpp container_lint.cpp

# Runtime stage: slim base, unprivileged user, no compiler.
FROM debian:12.7-slim
COPY --from=build /out/cppref-service /usr/local/bin/cppref-service
USER 10001:10001
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s CMD ["/usr/local/bin/cppref-service", "check-config"]
ENTRYPOINT ["/usr/local/bin/cppref-service"]
CMD ["serve"]
