*** Settings ***
Documentation       End-to-end tests of the TLS 1.3 echo server, running in Docker and driven from the host.
...
...                 = Virtualization =
...
...                 The server under test does not run on the host. ``Suite Setup`` of this directory runs
...                 ``docker compose up --build --wait`` with ``docker/docker-compose.yml``, which starts two
...                 containers from the image ``testserver_echo:e2e``:
...
...                 | =Service= | =Host port= | =Server options= | =Used by= |
...                 | ``echo`` | ``127.0.0.1:50445`` | ``--policy policies/tls13_pqc.txt --idle-timeout 3`` | ``echo.robot``, ``benchmark.robot`` |
...                 | ``echo-mtls`` | ``127.0.0.1:50446`` | ``--policy policies/tls13_pqc.txt --client-ca /certs/ca.pem`` | ``mtls.robot``, ``benchmark.robot`` |
...
...                 The image is built by ``docker/Dockerfile``, with the repository root as build context, in three stages:
...
...                 - ``deps``: Ubuntu 26.04 with Boost and Botan 3 built by ``3rdparty/install.sh``. This is the
...                 slow part (15 to 25 minutes the first time) and is cached by Docker afterwards, since it only
...                 depends on the install script.
...                 - ``build``: compiles ``testserver_echo`` from ``CMakeLists.txt`` and ``src/``, without the unit tests.
...                 - ``runtime``: a plain Ubuntu 26.04 with the binary, the two shared libraries and ``policies/``.
...
...                 Nothing is built or installed on the host for the server: the host only needs Docker.
...
...                 = Certificates =
...
...                 ``tests/fixtures`` is mounted read-only at ``/certs`` in both containers. The server uses
...                 ``server.pem`` and ``server.key`` (issued for ``localhost``); the clients on the host trust
...                 ``ca.pem`` and, for mutual TLS, present ``client.pem`` and ``client.key``. The containers run
...                 with the host's user and group id (``E2E_UID``, ``E2E_GID``), because the fixture keys are
...                 readable by their owner only.
...
...                 = Request path =
...
...                 Every request starts on the host and crosses the Docker port mapping into a container:
...
...                 | host client --TCP--> 127.0.0.1:5044x --docker-proxy/NAT--> container:5044x --TLS 1.3--> echo
...
...                 The clients are:
...
...                 - ``libraries/EchoClient.py``: Python's ``ssl`` module, that is the host's OpenSSL. Functional
...                 and mutual TLS tests.
...                 - ``openssl s_client``: checks the negotiated key exchange group.
...                 - ``scripts/benchmark.sh`` with ``botan tls_client``: the load test in ``benchmark.robot``.
...
...                 Ports are published on ``127.0.0.1`` only, so the servers are not reachable from the network.
...
...                 = Results =
...
...                 Besides Robot's ``log.html`` and ``report.html``, ``Suite Teardown`` writes the logs of both
...                 containers to ``docker-compose.log`` in the output directory, then runs ``docker compose down``.
...                 It runs even when a test fails, so no container is left behind.
...
...                 = Requirements =
...
...                 - Docker with Compose v2 (``docker compose``), usable without ``sudo``.
...                 - Python 3 with ``robotframework`` (``tests/e2e/requirements.txt``).
...                 - OpenSSL 3.5 or newer on the host for the hybrid post-quantum group test.
...                 - ``botan`` in the ``PATH`` for ``benchmark.robot``; without it those tests are skipped.
...
...                 = Running =
...
...                 | scripts/run-e2e.sh                        # everything
...                 | scripts/run-e2e.sh --exclude benchmark    # functional and mTLS tests only
...                 | scripts/run-e2e.sh --include benchmark --variable BENCH_CLIENTS:100

Resource            resources/e2e.resource

Suite Setup         Start Echo Containers
Suite Teardown      Stop Echo Containers And Save Logs

Test Timeout        5 min
