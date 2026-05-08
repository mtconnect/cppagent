#!/usr/bin/env bash
# Build and test the cppagent inside an ubuntu-22.04 container that mirrors
# the GitHub Actions build (.github/workflows/build-ubuntu-22.04.yml).
#
# Usage:
#   docker/build.sh                   # full conan create, matches CI
#   docker/build.sh shell             # interactive bash inside the container
#   docker/build.sh deb [version]     # package dist.tar.gz as mtconnect-agent_<version>_all.deb
#   docker/build.sh -- <cmd...>       # run an arbitrary command in the container
#
# The conan cache is persisted on the host at ~/.cache/cppagent-conan
# (override with CPPAGENT_CONAN_CACHE) so subsequent builds skip recompiling
# boost et al.

set -euo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly IMAGE_TAG="cppagent-build:ubuntu-22.04"
readonly DOCKERFILE="${REPO_ROOT}/docker/Dockerfile.ubuntu-22.04"
# Host-side conan cache. A host directory (rather than a named docker volume)
# is used so it ends up owned by the host user, letting the container's
# --user $(id -u):$(id -g) actually write to it without a chown step.
readonly CACHE_DIR="${CPPAGENT_CONAN_CACHE:-${HOME}/.cache/cppagent-conan}"

if [[ ! -f "${DOCKERFILE}" ]]; then
    echo "error: ${DOCKERFILE} not found" >&2
    exit 1
fi

# Build the image only when the Dockerfile is newer than the image, or the
# image doesn't exist yet. `docker build` is itself a no-op when nothing has
# changed, but skipping the call when possible avoids the daemon round-trip.
if ! docker image inspect "${IMAGE_TAG}" >/dev/null 2>&1; then
    echo ">>> Building ${IMAGE_TAG}"
    docker build -t "${IMAGE_TAG}" -f "${DOCKERFILE}" "${REPO_ROOT}/docker"
fi

# Make sure the conan cache directory exists on the host (will be owned by
# the host user so the container can write to it as that user).
mkdir -p "${CACHE_DIR}"

# The default command runs the same `conan create` invocation as CI. The
# LDFLAGS match the workflow so the produced binaries are statically linked
# against libgcc/libstdc++.
#
# The repo's conan/profiles/gcc still uses the old [system_tools] section
# name; conan 2.x renamed it to [platform_tool_requires]. The semantics are
# identical, so we copy the profile to a tmp file with the section renamed
# rather than touching the in-tree file.
# TODO: rename the section in conan/profiles/gcc itself and drop this sed.
readonly CI_BUILD_CMD='set -euo pipefail
export CTEST_OUTPUT_ON_FAILURE=TRUE
export LDFLAGS="-static-libgcc -static-libstdc++"
profile=$(mktemp)
sed "s/^\[system_tools\]/[platform_tool_requires]/" conan/profiles/gcc > "$profile"
conan profile detect -f
conan create . \
    --build=missing \
    -pr "$profile" \
    -o shared=False \
    -o development=True \
    -o cpack=True \
    -o cpack_name=dist \
    -o cpack_destination=/work \
    --test-folder='

# Replicates the GitHub `Prepare Debian Package` + `Create Debian Package`
# steps from .github/workflows/build-deb.yml, but locally. Stages the contents
# of dist.tar.gz under pkgroot/home/edge/agent, writes a DEBIAN/control file
# with the requested version, and runs dpkg-deb --build. Matches the same
# layout the jiro4989/build-deb-action@v3 action produces in CI.
#
# $1 - debian package version string (e.g. "2.6.0.2.2-dnx").
deb_build_cmd() {
    local version="$1"
    cat <<EOF
set -euo pipefail
if [[ ! -f /work/dist.tar.gz ]]; then
    echo "error: /work/dist.tar.gz missing — run 'docker/build.sh' first" >&2
    exit 1
fi
cd /work
rm -rf pkgroot/home pkgroot/DEBIAN/control
mkdir -p pkgroot/home/edge pkgroot/DEBIAN
tar -xzf dist.tar.gz -C pkgroot/home/edge/
mv pkgroot/home/edge/dist pkgroot/home/edge/agent
mv pkgroot/home/edge/agent/share/mtconnect/* pkgroot/home/edge/agent/
rm -rf pkgroot/home/edge/agent/share \
       pkgroot/home/edge/agent/docker \
       pkgroot/home/edge/agent/demo
# Generated dynamically — pkgroot/DEBIAN/control is intentionally not in git
# (see commit da3e934e) since CI builds it via jiro4989/build-deb-action.
cat > pkgroot/DEBIAN/control <<CTRL
Package: mtconnect-agent
Version: ${version}
Section: misc
Priority: optional
Architecture: all
Maintainer: Datanomix <support@datanomix.io>
Description: MTConnect Agent for Linux
CTRL
out="mtconnect-agent_${version}_all.deb"
dpkg-deb --build pkgroot "\$out"
echo ">>> built /work/\$out"
ls -lh "\$out"
EOF
}

# Pick the command to run inside the container.
if [[ $# -eq 0 ]]; then
    container_cmd=(bash -lc "${CI_BUILD_CMD}")
elif [[ "$1" == "shell" ]]; then
    container_cmd=(bash)
elif [[ "$1" == "deb" ]]; then
    deb_version="${2:-0.0.0.0}"
    container_cmd=(bash -lc "$(deb_build_cmd "${deb_version}")")
elif [[ "$1" == "--" ]]; then
    shift
    container_cmd=("$@")
else
    container_cmd=("$@")
fi

# Only attach a TTY if we have one (so the script also works under CI / nohup).
tty_flags=(--rm)
if [[ -t 0 && -t 1 ]]; then
    tty_flags+=(-it)
fi

# Run as the host user so build outputs aren't owned by root, and override
# HOME to the conan cache volume so conan can write its package cache there.
exec docker run "${tty_flags[@]}" \
    --user "$(id -u):$(id -g)" \
    -e HOME=/conan-home \
    -v "${REPO_ROOT}:/work" \
    -v "${CACHE_DIR}:/conan-home" \
    -w /work \
    "${IMAGE_TAG}" \
    "${container_cmd[@]}"
