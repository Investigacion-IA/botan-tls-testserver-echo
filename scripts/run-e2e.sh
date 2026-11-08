#!/usr/bin/env bash
# Runs the Robot Framework end-to-end suites in tests/e2e against the echo server in Docker.
#
#   scripts/run-e2e.sh                          # all suites
#   scripts/run-e2e.sh --exclude benchmark      # extra arguments go to robot
#
# Robot Framework is installed into .venv on the first run. Results go to build/e2e-results
# (override with E2E_OUTPUT_DIR).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="${ROOT}/.venv"
OUTPUT_DIR="${E2E_OUTPUT_DIR:-${ROOT}/build/e2e-results}"

command -v docker >/dev/null 2>&1 || { echo "error: docker not found" >&2; exit 1; }
docker compose version >/dev/null 2>&1 || { echo "error: docker compose (v2) not available" >&2; exit 1; }

if [[ ! -x "${VENV}/bin/robot" ]]; then
   python3 -m venv "${VENV}"
   "${VENV}/bin/pip" install --quiet --upgrade pip
fi
"${VENV}/bin/pip" install --quiet -r "${ROOT}/tests/e2e/requirements.txt"

exec "${VENV}/bin/robot" --outputdir "${OUTPUT_DIR}" --name "E2E" "$@" "${ROOT}/tests/e2e"
