#!/usr/bin/env bash
# Load test for the TLS 1.3 echo server, driven by Botan's own TLS client (`botan tls_client`).
#
# Starts N concurrent clients. Every client opens one TLS 1.3 connection, waits for the handshake, then sends
# ASCII text and waits for the same bytes to come back, ROUNDS times. The server handles all of them
# on its coroutines, so this shows how it behaves with many simultaneous connections.
#
# Reported: handshake latency, echo round-trip latency per round, connections per second, throughput, and
# whether every client got back exactly what it sent.
#
#   scripts/benchmark.sh --port 50445 --ca certs/ca.pem --clients 50 --rounds 20
#
# Each client is a separate `botan` process, so on a small machine the client side is the bottleneck
# long before the server is. Look at the latency columns, not only at the totals.
set -uo pipefail

HOST="localhost"      # must match the certificate (the example certificate is for "localhost")
PORT=""
CA="certs/ca.pem"
POLICY="policies/tls13_pqc.txt"
CLIENT_CERT=""
CLIENT_KEY=""
BOTAN="$(command -v botan || echo /usr/local/bin/botan)"
CLIENTS=10
ROUNDS=10
LINES=100
LINE_SIZE=64
TIMEOUT=30
KEEP=0

usage() {
   cat <<EOF
Usage: $0 --port PORT [options]

  --port PORT        echo server port (required)
  --host HOST        server name, must match the certificate        (default: ${HOST})
  --ca FILE          CA certificate that signed the server's        (default: ${CA})
  --policy FILE      Botan TLS policy for the clients               (default: ${POLICY})
  --client-cert FILE client certificate, for a server run with --client-ca
  --client-key FILE  private key of the client certificate
  --botan PATH       botan executable                               (default: ${BOTAN})
  --clients N        concurrent clients, one TLS connection each    (default: ${CLIENTS})
  --rounds N         send-and-wait rounds per client                (default: ${ROUNDS})
  --lines N          ASCII lines per round                          (default: ${LINES})
  --line-size N      bytes per line, including the newline          (default: ${LINE_SIZE})
  --timeout SECONDS  give up on a handshake or a round after this   (default: ${TIMEOUT})
  --keep             keep the working files (outputs and results)
  -h, --help         this text
EOF
}

die() {
   echo "error: $*" >&2
   exit 1
}

while [[ $# -gt 0 ]]; do
   case "$1" in
      --port) PORT="${2:?}"; shift 2 ;;
      --host) HOST="${2:?}"; shift 2 ;;
      --ca) CA="${2:?}"; shift 2 ;;
      --policy) POLICY="${2:?}"; shift 2 ;;
      --client-cert) CLIENT_CERT="${2:?}"; shift 2 ;;
      --client-key) CLIENT_KEY="${2:?}"; shift 2 ;;
      --botan) BOTAN="${2:?}"; shift 2 ;;
      --clients) CLIENTS="${2:?}"; shift 2 ;;
      --rounds) ROUNDS="${2:?}"; shift 2 ;;
      --lines) LINES="${2:?}"; shift 2 ;;
      --line-size) LINE_SIZE="${2:?}"; shift 2 ;;
      --timeout) TIMEOUT="${2:?}"; shift 2 ;;
      --keep) KEEP=1; shift ;;
      -h | --help) usage; exit 0 ;;
      *) usage >&2; die "unknown option: $1" ;;
   esac
done

[[ -n "${PORT}" ]] || { usage >&2; die "--port is required"; }
for value in "${CLIENTS}" "${ROUNDS}" "${LINES}" "${LINE_SIZE}" "${TIMEOUT}"; do
   [[ "${value}" =~ ^[0-9]+$ && "${value}" -ge 1 ]] || die "numeric options must be positive integers"
done
[[ "${LINE_SIZE}" -ge 2 ]] || die "--line-size must be at least 2"
[[ -x "${BOTAN}" ]] || die "botan not found (use --botan PATH)"
[[ -f "${CA}" ]] || die "CA file not found: ${CA} (use --ca FILE)"
[[ -f "${POLICY}" ]] || die "policy file not found: ${POLICY} (use --policy FILE)"
if [[ -n "${CLIENT_CERT}${CLIENT_KEY}" ]]; then
   [[ -f "${CLIENT_CERT}" && -f "${CLIENT_KEY}" ]] || die "--client-cert and --client-key must both be given and exist"
fi
for tool in stdbuf mkfifo awk cmp stat; do
   command -v "${tool}" >/dev/null 2>&1 || die "missing command: ${tool}"
done

WORKDIR="$(mktemp -d /tmp/echo-benchmark.XXXXXX)"
cleanup() {
   jobs -p | xargs -r kill 2>/dev/null
   pkill -P $$ 2>/dev/null || true
   if [[ "${KEEP}" -eq 0 ]]; then
      rm -rf "${WORKDIR}"
   else
      echo "working files kept in ${WORKDIR}"
   fi
}
trap cleanup EXIT

now_us() {
   local t="${EPOCHREALTIME/./}"
   echo "${t}"
}

# ASCII text, LINES lines of LINE_SIZE bytes (LINE_SIZE-1 characters and the newline).
awk -v lines="${LINES}" -v size="${LINE_SIZE}" 'BEGIN {
   srand(42)
   chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,;:-_/"
   n = length(chars)
   for (i = 0; i < lines; i++) {
      s = ""
      for (j = 0; j < size - 1; j++) {
         s = s substr(chars, int(rand() * n) + 1, 1)
      }
      print s
   }
}' >"${WORKDIR}/payload"

PAYLOAD_BYTES="$(stat -c %s "${WORKDIR}/payload")"
for ((r = 0; r < ROUNDS; r++)); do
   cat "${WORKDIR}/payload"
done >"${WORKDIR}/expected"
EXPECTED_BYTES=$((PAYLOAD_BYTES * ROUNDS))

# One client: connect, handshake, ROUNDS x (send payload, wait for it to come back), verify.
# Writes "H <ms>" (handshake), "R <ms>" (one per round) and "S ok|fail <why>" to result.<id>.
run_client() {
   local id="$1"
   local dir="${WORKDIR}/c${id}"
   local result="${WORKDIR}/result.${id}"
   mkdir -p "${dir}"
   mkfifo "${dir}/in"

   local deadline_us=$((TIMEOUT * 1000000))
   local t_start
   t_start="$(now_us)"

   local client_auth=()
   if [[ -n "${CLIENT_CERT}" ]]; then
      client_auth=(--client-cert="${CLIENT_CERT}" --client-cert-key="${CLIENT_KEY}")
   fi

   # -o0: the client must not buffer what it prints, or the echo would only show up at exit.
   stdbuf -o0 "${BOTAN}" tls_client "${HOST}" --port="${PORT}" --policy="${POLICY}" --trusted-cas="${CA}" \
      "${client_auth[@]}" <"${dir}/in" >"${dir}/out" 2>"${dir}/err" &
   local pid=$!
   exec 3>"${dir}/in"

   local offset=""
   while :; do
      if offset="$(grep -bx 'Handshake complete' "${dir}/out" 2>/dev/null | head -n1 | cut -d: -f1)" && [[ -n "${offset}" ]]; then
         offset=$((offset + 19)) # "Handshake complete\n"
         break
      fi
      if ! kill -0 "${pid}" 2>/dev/null; then
         echo "S fail handshake: client exited ($(head -c 200 "${dir}/err" | tr '\n' ' '))" >"${result}"
         exec 3>&-
         return
      fi
      if (($(now_us) - t_start > deadline_us)); then
         echo "S fail handshake: timeout" >"${result}"
         exec 3>&-
         kill "${pid}" 2>/dev/null
         return
      fi
      sleep 0.01
   done
   echo "H $((($(now_us) - t_start) / 1000))" >"${result}"

   local r
   for ((r = 1; r <= ROUNDS; r++)); do
      local t_round
      t_round="$(now_us)"
      cat "${WORKDIR}/payload" >&3
      local want=$((offset + r * PAYLOAD_BYTES))
      while (($(stat -c %s "${dir}/out") < want)); do
         if ! kill -0 "${pid}" 2>/dev/null; then
            echo "S fail round ${r}: client exited" >>"${result}"
            exec 3>&-
            return
         fi
         if (($(now_us) - t_round > deadline_us)); then
            echo "S fail round ${r}: timeout" >>"${result}"
            exec 3>&-
            kill "${pid}" 2>/dev/null
            return
         fi
         sleep 0.002
      done
      echo "R $((($(now_us) - t_round) / 1000))" >>"${result}"
   done

   exec 3>&-
   wait "${pid}" 2>/dev/null

   if tail -c +$((offset + 1)) "${dir}/out" | head -c "${EXPECTED_BYTES}" | cmp -s - "${WORKDIR}/expected"; then
      echo "S ok" >>"${result}"
   else
      echo "S fail echoed text differs from the text sent" >>"${result}"
   fi
}

# min / avg / p50 / p95 / max of the numbers on stdin.
stats() {
   sort -n | awk '{ v[NR] = $1; sum += $1 }
      END {
         if (NR == 0) { print "n/a"; exit }
         p50 = v[int((NR - 1) * 0.50) + 1]
         p95 = v[int((NR - 1) * 0.95) + 1]
         printf "min %d   avg %.1f   p50 %d   p95 %d   max %d", v[1], sum / NR, p50, p95, v[NR]
      }'
}

echo "Echo server benchmark with ${BOTAN##*/} tls_client"
echo "  target:   ${HOST}:${PORT}"
echo "  clients:  ${CLIENTS} concurrent, ${ROUNDS} rounds each$([[ -n "${CLIENT_CERT}" ]] && echo ', with client certificate')"
echo "  payload:  ${LINES} lines x ${LINE_SIZE} bytes = ${PAYLOAD_BYTES} bytes per round"
echo

T0="$(now_us)"
for ((i = 1; i <= CLIENTS; i++)); do
   run_client "${i}" &
done
wait
T1="$(now_us)"

WALL_MS=$(((T1 - T0) / 1000))
[[ "${WALL_MS}" -ge 1 ]] || WALL_MS=1

OK=0
FAILED=0
for ((i = 1; i <= CLIENTS; i++)); do
   if grep -q '^S ok' "${WORKDIR}/result.${i}" 2>/dev/null; then
      OK=$((OK + 1))
   else
      FAILED=$((FAILED + 1))
   fi
done

echo "Results"
echo "  clients ok:        ${OK} / ${CLIENTS}"
if [[ "${FAILED}" -gt 0 ]]; then
   echo "  failures:"
   grep -h '^S fail' "${WORKDIR}"/result.* 2>/dev/null | sort | uniq -c | sed 's/^/    /'
fi
echo "  handshake (ms):    $(cat "${WORKDIR}"/result.* 2>/dev/null | awk '$1 == "H" { print $2 }' | stats)"
echo "  echo round (ms):   $(cat "${WORKDIR}"/result.* 2>/dev/null | awk '$1 == "R" { print $2 }' | stats)"

ROUNDS_DONE="$(cat "${WORKDIR}"/result.* 2>/dev/null | awk '$1 == "R" { n++ } END { print n + 0 }')"
ECHOED=$((ROUNDS_DONE * PAYLOAD_BYTES))
echo "  total time:        ${WALL_MS} ms"
echo "  connections/s:     $(awk -v n="${OK}" -v ms="${WALL_MS}" 'BEGIN { printf "%.1f", n * 1000 / ms }')"
echo "  rounds/s:          $(awk -v n="${ROUNDS_DONE}" -v ms="${WALL_MS}" 'BEGIN { printf "%.1f", n * 1000 / ms }')"
echo "  echoed:            ${ECHOED} bytes ($(awk -v b="${ECHOED}" -v ms="${WALL_MS}" 'BEGIN { printf "%.2f", b / 1048576 * 1000 / ms }') MiB/s, same again in the other direction)"

[[ "${FAILED}" -eq 0 ]]
