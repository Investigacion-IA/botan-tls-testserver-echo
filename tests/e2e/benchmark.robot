*** Settings ***
Documentation       Load test of the containerized echo servers with ``scripts/benchmark.sh``.
...
...                 The script runs on the host and starts ``${BENCH_CLIENTS}`` concurrent ``botan tls_client``
...                 processes. Each opens one TLS 1.3 connection and echoes ``${BENCH_ROUNDS}`` rounds of
...                 ``${BENCH_LINES}`` lines of ``${BENCH_LINE_SIZE}`` bytes. A test passes when every client got
...                 back exactly what it sent. The handshake and round-trip latencies, the connection rate and
...                 the throughput are shown as the test message and in the log.
...
...                 Each client is a separate process on the host, so the numbers include the startup of those
...                 processes and the Docker port mapping; compare runs on the same machine. Change the load
...                 with ``--variable``, for example ``--variable BENCH_CLIENTS:100``.
...                 Skipped when ``botan`` is not available on the host.

Resource            resources/e2e.resource

Test Setup          Botan CLI Should Be Available
Test Tags           benchmark


*** Variables ***
${BENCH_CLIENTS}        20
${BENCH_ROUNDS}         10
${BENCH_LINES}          100
${BENCH_LINE_SIZE}      64
${BENCH_TIMEOUT}        30


*** Test Cases ***
Benchmark Without Client Certificates
    [Documentation]    Load on the ``echo`` container (``127.0.0.1:50445``).
    Run Benchmark    ${ECHO_PORT}

Benchmark With Client Certificates
    [Documentation]    Load on the ``echo-mtls`` container (``127.0.0.1:50446``), every client presenting
    ...    ``client.pem``.
    Run Benchmark    ${MTLS_PORT}    --client-cert    ${CLIENT_CERT}    --client-key    ${CLIENT_KEY}


*** Keywords ***
Run Benchmark
    [Documentation]    Runs ``scripts/benchmark.sh`` against ``port`` and fails unless every client succeeded.
    [Arguments]    ${port}    @{extra}
    ${result}=    Run Process    ${REPO_ROOT}/scripts/benchmark.sh
    ...    --host    ${SERVER_NAME}    --port    ${port}    --ca    ${CA_FILE}
    ...    --policy    ${REPO_ROOT}/policies/tls13_pqc.txt
    ...    --clients    ${BENCH_CLIENTS}    --rounds    ${BENCH_ROUNDS}
    ...    --lines    ${BENCH_LINES}    --line-size    ${BENCH_LINE_SIZE}    --timeout    ${BENCH_TIMEOUT}
    ...    @{extra}
    ...    cwd=${REPO_ROOT}    stderr=STDOUT    timeout=10 min
    Log    ${result.stdout}
    Should Match Regexp    ${result.stdout}    clients ok:\\s+${BENCH_CLIENTS} / ${BENCH_CLIENTS}\\b
    ...    msg=Not every client got its echo back:\n${result.stdout}    values=False
    Should Be Equal As Integers    ${result.rc}    0
    ...    msg=benchmark.sh failed (rc=${result.rc}):\n${result.stdout}    values=False
    ${metrics}=    Get Lines Matching Regexp    ${result.stdout}    ^\\s+(handshake|echo round|connections/s|rounds/s|echoed).*
    Set Test Message    ${metrics}
