#!/bin/sh
# make-voice-model.sh — downloads/ggml-tiny.en-acft-q8_0.bin, the dictation
# model: FUTO's ACFT tiny.en (https://huggingface.co/futo-org/acft-whisper-tiny.en,
# Apache 2.0, fine-tuned from OpenAI's MIT Whisper) converted to ggml with
# whisper.cpp's script and quantised to q8_0, as FUTO ships it. Needs Python 3
# and about 1 GB for PyTorch, in a throwaway virtualenv under build/.
set -eu
cd "$(dirname "$0")/.."
W=third_party/whisper.cpp
B=build/voice-model
mkdir -p "$B" downloads
[ -x "$B/venv/bin/python" ] || python3 -m venv "$B/venv"
"$B/venv/bin/pip" install -q --index-url https://download.pytorch.org/whl/cpu torch
"$B/venv/bin/pip" install -q transformers numpy safetensors huggingface_hub
"$B/venv/bin/hf" download futo-org/acft-whisper-tiny.en --local-dir "$B/acft-tiny-en"
[ -d "$B/openai-whisper" ] ||
	git clone -q --depth 1 https://github.com/openai/whisper.git "$B/openai-whisper"
"$B/venv/bin/python" $W/models/convert-h5-to-ggml.py "$B/acft-tiny-en" \
	"$B/openai-whisper" "$B"
cmake -S $W -B "$B/host" -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DWHISPER_BUILD_TESTS=OFF -DWHISPER_SDL2=OFF >/dev/null
cmake --build "$B/host" --target whisper-quantize
"$B/host/bin/whisper-quantize" "$B/ggml-model.bin" \
	downloads/ggml-tiny.en-acft-q8_0.bin q8_0
