"""Robot Framework library: a TLS client for the echo server, built on Python's ssl module.

The host's OpenSSL does the TLS, so these tests exercise the server against a client that is not Botan.
OpenSSL 3.5 or newer offers X25519MLKEM768 by default; older versions fall back to x25519.
"""

import concurrent.futures
import random
import socket
import ssl
import time

from robot.api import logger
from robot.api.deco import keyword, library

_VERSIONS = {
    "TLSv1.2": ssl.TLSVersion.TLSv1_2,
    "TLSv1.3": ssl.TLSVersion.TLSv1_3,
}


@library(scope="SUITE", auto_keywords=False)
class EchoClient:
    """Opens TLS connections to the echo server and checks what comes back.

    A suite works with one "current" connection at a time: `Open TLS Connection` replaces it and
    `Close TLS Connection` closes it. The keywords that need several connections open their own.
    """

    def __init__(self, server_name="localhost", ca_file=None, timeout=10.0):
        self._server_name = server_name
        self._ca_file = ca_file
        self._timeout = float(timeout)
        self._connection = None

    def _context(self, ca_file, client_cert, client_key, min_version, max_version):
        # Not ssl.create_default_context(): since Python 3.13 it sets VERIFY_X509_STRICT, which the
        # throw-away test CA does not satisfy.
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.load_verify_locations(cafile=ca_file or self._ca_file)
        context.minimum_version = _VERSIONS[min_version]
        context.maximum_version = _VERSIONS[max_version]
        if client_cert:
            context.load_cert_chain(certfile=client_cert, keyfile=client_key)
        return context

    def _connect(self, host, port, ca_file=None, client_cert=None, client_key=None,
                 min_version="TLSv1.3", max_version="TLSv1.3"):
        context = self._context(ca_file, client_cert, client_key, min_version, max_version)
        raw = socket.create_connection((host, int(port)), timeout=self._timeout)
        try:
            return context.wrap_socket(raw, server_hostname=self._server_name)
        except BaseException:
            raw.close()
            raise

    @staticmethod
    def _echo(connection, data):
        connection.sendall(data)
        received = bytearray()
        while len(received) < len(data):
            chunk = connection.recv(min(65536, len(data) - len(received)))
            if not chunk:
                raise AssertionError(f"Connection closed after {len(received)} of {len(data)} echoed bytes")
            received.extend(chunk)
        return bytes(received)

    @keyword
    def wait_until_server_accepts_tls(self, host, port, timeout=60, ca_file=None,
                                      client_cert=None, client_key=None):
        """Retries a TLS 1.3 handshake with `host`:`port` until it succeeds or `timeout` seconds pass."""
        deadline = time.monotonic() + float(timeout)
        last_error = None
        while time.monotonic() < deadline:
            try:
                with self._connect(host, port, ca_file, client_cert, client_key) as connection:
                    logger.info(f"{host}:{port} accepted a {connection.version()} handshake")
                    return
            except (OSError, ssl.SSLError) as error:
                last_error = error
                time.sleep(0.5)
        raise AssertionError(f"{host}:{port} did not accept TLS within {timeout} s: {last_error}")

    @keyword
    def open_tls_connection(self, host, port, ca_file=None, client_cert=None, client_key=None,
                            min_version="TLSv1.3", max_version="TLSv1.3"):
        """Opens the current connection and completes the handshake. Returns the negotiated cipher suite."""
        self.close_tls_connection()
        self._connection = self._connect(host, port, ca_file, client_cert, client_key, min_version, max_version)
        cipher = self._connection.cipher()
        logger.info(f"Connected with {self._connection.version()}, {cipher[0]}")
        return cipher[0]

    @keyword
    def close_tls_connection(self):
        """Closes the current connection, if there is one."""
        if self._connection is not None:
            try:
                self._connection.close()
            finally:
                self._connection = None

    @keyword
    def negotiated_version_should_be(self, expected):
        """Fails unless the current connection uses protocol `expected`, for example `TLSv1.3`."""
        actual = self._current().version()
        if actual != expected:
            raise AssertionError(f"Negotiated {actual}, expected {expected}")

    @keyword
    def generate_payload(self, size, binary=True, seed=42):
        """Returns `size` deterministic bytes: every byte value if `binary`, printable ASCII lines otherwise."""
        size = int(size)
        rng = random.Random(int(seed))
        if binary:
            return rng.randbytes(size)
        alphabet = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,;:-_/"
        data = bytearray(rng.choice(alphabet) for _ in range(size))
        for i in range(63, size, 64):
            data[i] = ord("\n")
        return bytes(data)

    @keyword
    def send_and_receive_echo(self, data):
        """Sends `data` (str or bytes) on the current connection and returns as many bytes as were sent."""
        if isinstance(data, str):
            data = data.encode()
        return self._echo(self._current(), data)

    @keyword
    def echo_should_match(self, data):
        """Sends `data` and fails unless exactly the same bytes come back."""
        if isinstance(data, str):
            data = data.encode()
        received = self.send_and_receive_echo(data)
        if received != data:
            first = next(i for i, (a, b) in enumerate(zip(data, received)) if a != b)
            raise AssertionError(f"Echo differs from what was sent at byte {first} of {len(data)}")
        logger.info(f"{len(data)} bytes echoed unchanged")

    @keyword
    def connection_should_be_closed_by_peer_within(self, seconds):
        """Waits without sending and fails unless the server closes the current connection within
        `seconds`. Returns how long it took, in seconds."""
        connection = self._current()
        connection.settimeout(float(seconds))
        start = time.monotonic()
        try:
            while connection.recv(4096):
                pass
        except TimeoutError as error:
            raise AssertionError(f"The server kept the connection open for more than {seconds} s") from error
        except (ssl.SSLError, ConnectionResetError):
            pass
        finally:
            self.close_tls_connection()
        elapsed = time.monotonic() - start
        logger.info(f"Server closed the connection after {elapsed:.2f} s")
        return elapsed

    @keyword
    def handshake_should_fail(self, host, port, max_version="TLSv1.2", min_version="TLSv1.2"):
        """Fails if a handshake limited to `min_version`..`max_version` succeeds. Returns the client's error."""
        try:
            connection = self._connect(host, port, min_version=min_version, max_version=max_version)
        except (ssl.SSLError, ConnectionResetError) as error:
            logger.info(f"Handshake refused: {error}")
            return str(error)
        version = connection.version()
        connection.close()
        raise AssertionError(f"The server accepted a {version} handshake")

    @keyword
    def echo_should_be_refused(self, host, port, ca_file=None, client_cert=None, client_key=None):
        """Fails if the server echoes data on a new connection. Returns the client's error.

        In TLS 1.3 the server checks the client certificate after the client has finished its side of
        the handshake, so a refusal can show up either during the handshake or on the first read.
        """
        try:
            with self._connect(host, port, ca_file, client_cert, client_key) as connection:
                connection.sendall(b"ping\n")
                if connection.recv(5):
                    raise AssertionError("The server echoed data")
                return "connection closed without echo"
        except (ssl.SSLError, ConnectionResetError, BrokenPipeError) as error:
            logger.info(f"Refused: {error}")
            return str(error)

    @keyword
    def run_concurrent_echo_clients(self, host, port, clients=50, rounds=5, payload_size=16384,
                                    ca_file=None, client_cert=None, client_key=None):
        """Opens `clients` connections at the same time; each echoes `rounds` payloads of `payload_size`
        random bytes and checks them. Fails if any client fails. Returns the wall time in seconds."""
        clients = int(clients)
        rounds = int(rounds)
        payload_size = int(payload_size)

        def one_client(index):
            payload = random.Random(index).randbytes(payload_size)
            with self._connect(host, port, ca_file, client_cert, client_key) as connection:
                for _ in range(rounds):
                    if self._echo(connection, payload) != payload:
                        raise AssertionError(f"client {index}: echo differs from what was sent")

        start = time.monotonic()
        errors = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=clients) as pool:
            futures = {pool.submit(one_client, i): i for i in range(clients)}
            for future in concurrent.futures.as_completed(futures):
                if future.exception() is not None:
                    errors.append(f"client {futures[future]}: {future.exception()!r}")
        elapsed = time.monotonic() - start

        if errors:
            raise AssertionError(f"{len(errors)} of {clients} clients failed:\n" + "\n".join(errors[:10]))
        total = clients * rounds * payload_size
        logger.info(f"{clients} clients x {rounds} rounds x {payload_size} bytes = {total} bytes echoed "
                    f"in {elapsed:.2f} s")
        return round(elapsed, 3)

    def _current(self):
        if self._connection is None:
            raise AssertionError("No open TLS connection, use Open TLS Connection first")
        return self._connection
