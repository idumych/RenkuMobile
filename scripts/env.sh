# Sourced by the build scripts: where everything lives.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ARCH=${ARCH:-x86_64}
ISO=${ISO:-$(ls -t "$ROOT"/downloads/RenkuOS-x86_64-*.iso | head -1)}
REPO_TAR=${REPO_TAR:-$ROOT/downloads/renkuos-x86_64-repo.tar}
HOST_TOOLS=$ROOT/third_party/renku-src/generated.host/objects/linux/x86_64/release/tools
CROSS=$ROOT/third_party/renku-src/generated.$ARCH/cross-tools-$ARCH/bin
SYSROOT=$ROOT/build/sysroot-$ARCH
BFS_SHELL=$HOST_TOOLS/bfs_shell/bfs_shell
PACKAGE=$HOST_TOOLS/package/package
export LD_LIBRARY_PATH=$ROOT/third_party/renku-src/generated.host/objects/linux/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
# The BFS partition of an anyboot image starts at sector 12288.
BFS_OFFSET=$((12288 * 512))
