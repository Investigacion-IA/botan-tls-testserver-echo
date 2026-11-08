*** Settings ***
Documentation       Variables and keywords shared by the end-to-end suites.
...
...                 The echo servers run in Docker (see ``tests/e2e/docker-compose.yml``) and publish their
...                 ports on ``127.0.0.1``; every request in these suites is made from the host.

Library             OperatingSystem
Library             Process
Library             String
Library             ../libraries/EchoClient.py    server_name=${SERVER_NAME}    ca_file=${CA_FILE}


*** Variables ***
${REPO_ROOT}            ${{ os.path.normpath(r'${CURDIR}/../../..') }}
${E2E_DIR}              ${REPO_ROOT}/tests/e2e
${COMPOSE_FILE}         ${E2E_DIR}/docker-compose.yml
${FIXTURES}             ${REPO_ROOT}/tests/fixtures
${CA_FILE}              ${FIXTURES}/ca.pem
${CLIENT_CERT}          ${FIXTURES}/client.pem
${CLIENT_KEY}           ${FIXTURES}/client.key
# The test certificates are issued for "localhost"; the containers are reached on 127.0.0.1.
${SERVER_NAME}          localhost
${HOST}                 127.0.0.1
${ECHO_PORT}            50445
${MTLS_PORT}            50446
# Must match --idle-timeout of the "echo" service in docker-compose.yml.
${IDLE_TIMEOUT}         ${3}
# The first build compiles Boost and Botan inside Docker; later builds come from the layer cache.
${BUILD_TIMEOUT}        60 min
${STARTUP_TIMEOUT}      60


*** Keywords ***
Docker Compose
    [Documentation]    Runs ``docker compose`` with the e2e compose file and fails if it fails.
    ...    Returns the process result.
    [Arguments]    @{args}    ${timeout}=5 min
    ${uid}=    Evaluate    os.getuid()    modules=os
    ${gid}=    Evaluate    os.getgid()    modules=os
    ${result}=    Run Process    docker    compose    -f    ${COMPOSE_FILE}    @{args}
    ...    cwd=${E2E_DIR}    timeout=${timeout}    stderr=STDOUT
    ...    env:E2E_UID=${uid}    env:E2E_GID=${gid}
    Log    ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0
    ...    docker compose ${args} failed (rc=${result.rc}):\n${result.stdout}    values=False
    RETURN    ${result}

Start Echo Containers
    [Documentation]    Builds the image if needed, starts both echo servers and waits until they
    ...    complete a TLS 1.3 handshake from the host.
    Docker Compose    up    --detach    --build    --wait    --remove-orphans    timeout=${BUILD_TIMEOUT}
    Wait Until Server Accepts TLS    ${HOST}    ${ECHO_PORT}    timeout=${STARTUP_TIMEOUT}
    Wait Until Server Accepts TLS    ${HOST}    ${MTLS_PORT}    timeout=${STARTUP_TIMEOUT}
    ...    client_cert=${CLIENT_CERT}    client_key=${CLIENT_KEY}

Stop Echo Containers And Save Logs
    [Documentation]    Saves the logs of both containers to ``docker-compose.log`` in the output
    ...    directory, then removes the containers and their network.
    ${logs}=    Run Process    docker    compose    -f    ${COMPOSE_FILE}    logs    --no-color    --timestamps
    ...    cwd=${E2E_DIR}    stderr=STDOUT
    Create File    ${OUTPUT DIR}/docker-compose.log    ${logs.stdout}
    Docker Compose    down    --remove-orphans    --timeout    5

Server Log Should Contain
    [Documentation]    Fails unless the log of compose service ``service`` contains ``text``
    ...    (case-insensitive).
    [Arguments]    ${service}    ${text}
    ${result}=    Docker Compose    logs    --no-color    ${service}
    Should Contain    ${result.stdout}    ${text}    ignore_case=True
    ...    msg=The log of ${service} does not contain '${text}' (full log in docker-compose.log)    values=False

Botan CLI Should Be Available
    [Documentation]    Skips the current test when ``botan`` is not in the host's PATH.
    ${botan}=    Evaluate    shutil.which('botan') or (os.access('/usr/local/bin/botan', os.X_OK) and '/usr/local/bin/botan')
    ...    modules=shutil,os
    Skip If    not $botan    botan CLI not found on the host, needed by scripts/benchmark.sh
