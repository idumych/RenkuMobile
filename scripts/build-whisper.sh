#!/bin/sh
# build-whisper.sh — whisper.cpp (third_party/whisper.cpp, see README) as static
# libraries for Renku, in build/whisper-$ARCH, for the keyboard's dictation.
# Built for AVX2 CPUs (VoiceInput::Available() checks before use).
set -eu
. "$(dirname "$0")/env.sh"
SRC=$ROOT/third_party/whisper.cpp
OUT=$ROOT/build/whisper-$ARCH
mkdir -p "$OUT"

TOOLCHAIN=
if [ "$(uname -s)" != Haiku ]; then
	# Renku's cross compiler, with the system headers as the Makefile adds them
	H=$SYSROOT/system/develop/headers
	INC=$(for d in "$H/posix" "$H" "$H/os" "$H"/os/*/ "$H"/os/add-ons/*/; do
		printf -- '-idirafter %s ' "$d"; done)
	TOOLCHAIN=$OUT/toolchain.cmake
	cat > "$TOOLCHAIN" <<EOF
set(CMAKE_SYSTEM_NAME Haiku)
set(CMAKE_SYSTEM_PROCESSOR $ARCH)
set(CMAKE_C_COMPILER $CROSS/$ARCH-unknown-haiku-gcc)
set(CMAKE_CXX_COMPILER $CROSS/$ARCH-unknown-haiku-g++)
set(CMAKE_C_FLAGS_INIT "$INC")
set(CMAKE_CXX_FLAGS_INIT "$INC")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-B$SYSROOT/system/develop/lib -L$SYSROOT/system/develop/lib -L$SYSROOT/system/lib -Wl,-rpath-link=$SYSROOT/system/lib")
set(CMAKE_FIND_ROOT_PATH $SYSROOT/system)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
EOF
fi

cmake -S "$SRC" -B "$OUT" -G Ninja ${TOOLCHAIN:+-DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN} \
	-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DGGML_NATIVE=OFF \
	-DGGML_AVX=ON -DGGML_AVX2=ON -DGGML_FMA=ON -DGGML_F16C=ON -DGGML_OPENMP=OFF \
	-DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF \
	-DWHISPER_BUILD_SERVER=OFF -DWHISPER_SDL2=OFF
cmake --build "$OUT" --target whisper
