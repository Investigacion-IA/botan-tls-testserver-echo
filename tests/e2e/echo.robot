*** Settings ***
Documentation       Functional tests against the ``echo`` container (``127.0.0.1:50445``), without client certificates.
...
...                 Covers the TLS 1.3 handshake, the echo of text and binary data, the hybrid post-quantum
...                 key exchange, the refusal of TLS 1.2, the idle timeout and many concurrent connections.
...                 The containers are started by ``__init__.robot``.

Resource            resources/e2e.resource

Test Teardown       Close TLS Connection
Test Tags           functional


*** Test Cases ***
Handshake Negotiates TLS 1.3
    [Documentation]    A client that accepts TLS 1.2 and 1.3 ends up with TLS 1.3 and a TLS 1.3 cipher suite.
    ${cipher}=    Open TLS Connection    ${HOST}    ${ECHO_PORT}    min_version=TLSv1.2
    Negotiated Version Should Be    TLSv1.3
    Should Start With    ${cipher}    TLS_

Text Line Is Echoed
    [Documentation]    One line of ASCII text comes back unchanged.
    Open TLS Connection    ${HOST}    ${ECHO_PORT}
    Echo Should Match    Hello from the host, through Docker\n

Several Messages On One Connection Are Echoed
    [Documentation]    The connection stays usable: several messages in a row each come back in order.
    Open TLS Connection    ${HOST}    ${ECHO_PORT}
    FOR    ${i}    IN RANGE    20
        Echo Should Match    message ${i}\n
    END

One MiB Of Binary Data Is Echoed
    [Documentation]    Every byte value, 1 MiB in one write, comes back unchanged. The echo is not line based.
    ${payload}=    Generate Payload    1048576    binary=${True}
    Open TLS Connection    ${HOST}    ${ECHO_PORT}
    Echo Should Match    ${payload}

Hybrid Post-Quantum Group Is Negotiated
    [Documentation]    ``openssl s_client`` offering only ``X25519MLKEM768`` completes the handshake with it,
    ...    since ``policies/tls13_pqc.txt`` puts that group first. Needs OpenSSL 3.5 or newer on the host.
    [Tags]    pqc
    ${result}=    Run Process
    ...    openssl s_client -connect ${HOST}:${ECHO_PORT} -servername ${SERVER_NAME} -CAfile ${CA_FILE} -verify_return_error -tls1_3 -groups X25519MLKEM768 </dev/null
    ...    shell=True    stderr=STDOUT    timeout=30 s
    Log    ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0
    Should Contain    ${result.stdout}    Negotiated TLS1.3 group: X25519MLKEM768
    Should Contain    ${result.stdout}    Verify return code: 0 (ok)

TLS 1.2 Client Is Refused
    [Documentation]    A client limited to TLS 1.2 gets a protocol version alert, and the server logs the
    ...    failed handshake.
    ${error}=    Handshake Should Fail    ${HOST}    ${ECHO_PORT}    max_version=TLSv1.2
    Should Contain    ${error}    PROTOCOL_VERSION    ignore_case=True
    Wait Until Keyword Succeeds    10 s    0.5 s
    ...    Server Log Should Contain    echo    protocol_version

Idle Connection Is Closed By The Server
    [Documentation]    After one echo the client stays silent; the server closes the connection after its
    ...    idle timeout (``--idle-timeout ${IDLE_TIMEOUT}`` in ``docker/docker-compose.yml``), not before.
    Open TLS Connection    ${HOST}    ${ECHO_PORT}
    Echo Should Match    still there?\n
    ${elapsed}=    Connection Should Be Closed By Peer Within    ${IDLE_TIMEOUT + 5}
    Should Be True    ${elapsed} >= ${IDLE_TIMEOUT} - 0.5
    ...    Closed after ${elapsed} s, before the ${IDLE_TIMEOUT} s idle timeout

Concurrent Clients Are All Echoed
    [Documentation]    50 connections at the same time, each echoing 5 x 16 KiB of random bytes, all succeed.
    ${elapsed}=    Run Concurrent Echo Clients    ${HOST}    ${ECHO_PORT}    clients=50    rounds=5    payload_size=16384
    Set Test Message    50 clients x 5 rounds x 16 KiB in ${elapsed} s
