# Sourced by the build scripts: where everything lives.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ARCH=${ARCH:-x86_64}
ISO=${ISO:-$(ls -t "$ROOT"/downloads/RenkuOS-x86_64-*.iso | head -1)}
REPO_TAR=${REPO_TAR:-$ROOT/downloads/renkuos-x86_64-repo.tar}
# A Renku build directory's host tools (bfs_shell, package) and their libraries
HOST_BUILD=${HOST_BUILD:-$ROOT/third_party/renku-src/generated.host}
HOST_TOOLS=$HOST_BUILD/objects/linux/x86_64/release/tools
CROSS=${CROSS:-$ROOT/third_party/renku-src/generated.$ARCH/cross-tools-$ARCH/bin}
SYSROOT=${SYSROOT:-$ROOT/build/sysroot-$ARCH}
# The compilers' prefix: arm64 Haiku's is aarch64-unknown-haiku
case $ARCH in
arm64)	TRIPLE=aarch64-unknown-haiku ;;
*)		TRIPLE=$ARCH-unknown-haiku ;;
esac
BFS_SHELL=$HOST_TOOLS/bfs_shell/bfs_shell
PACKAGE=$HOST_TOOLS/package/package
export LD_LIBRARY_PATH=$HOST_BUILD/objects/linux/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
# The BFS partition of an anyboot image starts at sector 12288.
BFS_OFFSET=$((12288 * 512))
