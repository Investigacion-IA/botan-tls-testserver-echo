# TLS 1.3 echo server image, built from scratch: Boost and Botan 3 come from 3rdparty/install.sh.
# The build context is the repository root; Dockerfile.dockerignore next to this file limits it.
#
#   docker build -f docker/Dockerfile -t testserver_echo .
#   docker run --rm -p 50445:50445 -v "$PWD/tests/fixtures:/certs:ro" --user "$(id -u):$(id -g)" testserver_echo \
#      --port 50445 --cert /certs/server.pem --key /certs/server.key --policy policies/tls13_pqc.txt

ARG UBUNTU=ubuntu:26.04

# Boost and Botan take most of the build time, so this stage only depends on the install script.
FROM ${UBUNTU} AS deps
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential ca-certificates cmake curl python3 xz-utils \
 && rm -rf /var/lib/apt/lists/*
COPY 3rdparty/install.sh /tmp/3rdparty/install.sh
RUN /tmp/3rdparty/install.sh && rm -rf /tmp/3rdparty

FROM deps AS build
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
 && cmake --build build -j"$(nproc)"

FROM ${UBUNTU} AS runtime
COPY --from=deps /usr/local/lib/libbotan-3.so* /usr/local/lib/libboost_program_options.so* /usr/local/lib/
RUN ldconfig
WORKDIR /opt/testserver_echo
COPY --from=build /src/build/testserver_echo /usr/local/bin/testserver_echo
COPY policies ./policies
USER ubuntu
EXPOSE 50445
ENTRYPOINT ["testserver_echo"]
