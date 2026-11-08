*** Settings ***
Documentation       Mutual TLS against the ``echo-mtls`` container (``127.0.0.1:50446``, ``--client-ca /certs/ca.pem``).
...
...                 The server only echoes for clients that present a certificate signed by the test CA.
...                 The containers are started by ``__init__.robot``.

Resource            resources/e2e.resource

Test Teardown       Close TLS Connection
Test Tags           mtls


*** Test Cases ***
Client With Certificate Is Echoed
    [Documentation]    With ``client.pem`` and ``client.key`` the handshake succeeds and data is echoed.
    Open TLS Connection    ${HOST}    ${MTLS_PORT}    client_cert=${CLIENT_CERT}    client_key=${CLIENT_KEY}
    Negotiated Version Should Be    TLSv1.3
    Echo Should Match    authenticated hello\n

Client With Certificate Echoes Binary Data
    [Documentation]    256 KiB of binary data come back unchanged over a mutually authenticated connection.
    ${payload}=    Generate Payload    262144    binary=${True}
    Open TLS Connection    ${HOST}    ${MTLS_PORT}    client_cert=${CLIENT_CERT}    client_key=${CLIENT_KEY}
    Echo Should Match    ${payload}

Client Without Certificate Is Refused
    [Documentation]    Without a client certificate the server ends the connection and echoes nothing.
    ...    In TLS 1.3 the client may only see the refusal on its first read.
    Echo Should Be Refused    ${HOST}    ${MTLS_PORT}

Concurrent Clients With Certificates Are All Echoed
    [Documentation]    20 mutually authenticated connections at the same time, each echoing 5 x 16 KiB.
    ${elapsed}=    Run Concurrent Echo Clients    ${HOST}    ${MTLS_PORT}    clients=20    rounds=5    payload_size=16384
    ...    client_cert=${CLIENT_CERT}    client_key=${CLIENT_KEY}
    Set Test Message    20 clients x 5 rounds x 16 KiB in ${elapsed} s
