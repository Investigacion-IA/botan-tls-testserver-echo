#!/usr/bin/env bash
# Download the latest Botan 3 and Boost, build them, and install into /usr/local.
# GoogleTest and GoogleMock (for the unit tests) come from apt.
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${ROOT}/src"
JOBS="$(nproc 2>/dev/null || echo 1)"

if [[ "$(id -u)" -eq 0 ]]; then
   SUDO=()
else
   SUDO=(sudo)
fi

need() {
   if ! command -v "$1" >/dev/null 2>&1; then
      echo "Falta el comando '$1'" >&2
      exit 1
   fi
}

need curl
need python3
need tar
need g++
need make
need cmake

mkdir -p "${SRC}"

fetch() {
   local url="$1"
   local dest="$2"
   echo "Descargando ${url}"
   curl -fL --retry 3 -A "botan-tls-testserver-echo" -o "${dest}" "${url}"
}

extract() {
   local archive="$1"
   local dest="$2"
   rm -rf "${dest}"
   mkdir -p "${dest}"
   tar -xf "${archive}" -C "${dest}" --strip-components=1
}

latest_botan3_tag() {
   # Botan publica tags, no GitHub Releases.
   curl -fsSL -A "botan-tls-testserver-echo" -H "Accept: application/vnd.github+json" \
      "https://api.github.com/repos/randombit/botan/tags?per_page=100" |
      python3 -c '
import json, re, sys
versions = []
for tag in json.load(sys.stdin):
    name = tag["name"].lstrip("v")
    if re.fullmatch(r"3\.\d+\.\d+", name):
        versions.append(tuple(int(part) for part in name.split(".")))
if not versions:
    sys.exit("No hay un tag Botan 3")
print(".".join(str(part) for part in max(versions)))
'
}

latest_boost() {
   curl -fsSL -A "botan-tls-testserver-echo" -H "Accept: application/vnd.github+json" \
      "https://api.github.com/repos/boostorg/boost/releases/latest" |
      python3 -c '
import json, sys
release = json.load(sys.stdin)
tag = release["tag_name"]
url = ""
assets = release.get("assets", [])
for suffix in (".tar.xz", ".tar.gz", ".tar.bz2"):
    for asset in assets:
        name = asset["name"]
        if "b2-nodocs" in name and name.endswith(suffix):
            url = asset["browser_download_url"]
            break
    if url:
        break
print(tag)
print(url)
'
}

echo "==> Boost (latest)"
mapfile -t boost_meta < <(latest_boost)
BOOST_TAG="${boost_meta[0]}"
BOOST_URL="${boost_meta[1]:-}"
BOOST_VER="${BOOST_TAG#boost-}"
BOOST_VER="${BOOST_VER#v}"
if [[ -z "${BOOST_URL}" ]]; then
   BOOST_UNDER="${BOOST_VER//./_}"
   BOOST_URL="https://archives.boost.io/release/${BOOST_VER}/source/boost_${BOOST_UNDER}.tar.gz"
fi
BOOST_ARCHIVE="${SRC}/$(basename "${BOOST_URL}")"
fetch "${BOOST_URL}" "${BOOST_ARCHIVE}"
extract "${BOOST_ARCHIVE}" "${SRC}/boost"
(
   cd "${SRC}/boost"
   ./bootstrap.sh --prefix="${PREFIX}" --with-libraries=program_options
   ./b2 -j"${JOBS}" cxxstd=20 variant=release link=shared,static threading=multi --prefix="${PREFIX}"
   "${SUDO[@]}" ./b2 -j"${JOBS}" cxxstd=20 variant=release link=shared,static threading=multi --prefix="${PREFIX}" install
)

echo "==> Botan 3 (latest)"
BOTAN_TAG="$(latest_botan3_tag)"
BOTAN_ARCHIVE="${SRC}/botan-${BOTAN_TAG}.tar.gz"
fetch "https://github.com/randombit/botan/archive/refs/tags/${BOTAN_TAG}.tar.gz" "${BOTAN_ARCHIVE}"
extract "${BOTAN_ARCHIVE}" "${SRC}/botan"
(
   cd "${SRC}/botan"
   # Botan 3 detecta Boost por la ruta de includes del compilador.
   export CPATH="${PREFIX}/include${CPATH:+:${CPATH}}"
   export LIBRARY_PATH="${PREFIX}/lib${LIBRARY_PATH:+:${LIBRARY_PATH}}"
   ./configure.py \
      --prefix="${PREFIX}" \
      --with-boost \
      --without-documentation
   make -j"${JOBS}"
   "${SUDO[@]}" make install
)

echo "==> GoogleTest / GoogleMock (unit tests)"
export DEBIAN_FRONTEND=noninteractive
"${SUDO[@]}" apt-get update
"${SUDO[@]}" apt-get install -y --no-install-recommends libgtest-dev libgmock-dev

if command -v ldconfig >/dev/null 2>&1; then
   "${SUDO[@]}" ldconfig
fi

echo "Instalado en ${PREFIX}: Boost ${BOOST_VER}, Botan ${BOTAN_TAG}"
