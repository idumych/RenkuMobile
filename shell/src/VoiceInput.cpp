/*
 * VoiceInput: see VoiceInput.h.
 */
#include "VoiceInput.h"

#include <Autolock.h>
#include <ByteOrder.h>
#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <MediaRecorder.h>
#include <MediaRoster.h>
#include <OS.h>
#include <Path.h>

#include <whisper.h>

#include <algorithm>
#include <math.h>
#include <sys/stat.h>


static const float kRate = 16000;
// FUTO's: loudness is judged 100 ms at a time.
static const size_t kChunk = 1600;
static const size_t kIgnoreStart = 9600;	// 0.6 s
static const size_t kBlockedAfter = 32000;	// 2 s
static const int32 kQuietChunksToStop = 20;	// 2 s
static const float kSpeechRMS = 0.01f;
static const float kAnyNoiseRMS = 0.0001f;
// Whisper hears at most 30 seconds.
static const size_t kMaxSamples = 30 * 16000;


static BString
model_path()
{
	BPath path;
	if (find_directory(B_SYSTEM_DATA_DIRECTORY, &path) != B_OK
		|| path.Append("renku_mobile/ggml-tiny.en-acft-q8_0.bin") != B_OK)
		return "";
	return path.Path();
}


/*!	Keeps what was last dictated, as heard (16 kHz mono 16-bit WAV), in
	~/config/settings/Renku Mobile/last dictation.wav: if the words come out
	wrong, it tells a poor recording from a mishearing.
*/
static void
save_recording(const std::vector<float>& samples)
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) != B_OK
		|| path.Append("Renku Mobile") != B_OK
		|| create_directory(path.Path(), 0755) != B_OK
		|| path.Append("last dictation.wav") != B_OK)
		return;
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	uint32 dataSize = samples.size() * 2;
	struct {
		char	riff[4];
		uint32	riffSize;
		char	wave[4];
		char	fmt[4];
		uint32	fmtSize;
		uint16	format;
		uint16	channels;
		uint32	rate;
		uint32	byteRate;
		uint16	blockAlign;
		uint16	bits;
		char	data[4];
		uint32	dataSize;
	} _PACKED header = {
		{ 'R', 'I', 'F', 'F' }, B_HOST_TO_LENDIAN_INT32(36 + dataSize),
		{ 'W', 'A', 'V', 'E' }, { 'f', 'm', 't', ' ' },
		B_HOST_TO_LENDIAN_INT32(16), B_HOST_TO_LENDIAN_INT16(1),
		B_HOST_TO_LENDIAN_INT16(1), B_HOST_TO_LENDIAN_INT32((uint32)kRate),
		B_HOST_TO_LENDIAN_INT32((uint32)kRate * 2),
		B_HOST_TO_LENDIAN_INT16(2), B_HOST_TO_LENDIAN_INT16(16),
		{ 'd', 'a', 't', 'a' }, B_HOST_TO_LENDIAN_INT32(dataSize)
	};
	std::vector<int16> pcm(samples.size());
	for (size_t i = 0; i < samples.size(); i++) {
		pcm[i] = B_HOST_TO_LENDIAN_INT16((int16)std::clamp(
			samples[i] * 32767.0f, -32768.0f, 32767.0f));
	}
	file.Write(&header, sizeof(header));
	file.Write(pcm.data(), dataSize);
	// out to the disk now, in case the device is simply switched off
	file.Sync();
}


VoiceInput::VoiceInput()
	:
	fRecorder(NULL),
	fLock("voice input"),
	fContext(NULL)
{
}


VoiceInput::~VoiceInput()
{
	Stop();
	delete fRecorder;
	if (fContext != NULL)
		whisper_free(fContext);
}


/*static*/ bool
VoiceInput::Available()
{
	struct stat st;
	if (stat(model_path().String(), &st) != 0)
		return false;
#if defined(__x86_64__) || defined(__i386__)
	// shortcut: whisper.cpp is built for AVX2 only; a build of ggml's CPU
	// variants (GGML_BACKEND_DL) would bring dictation to older x86 CPUs.
	return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
	// arm64: ggml's NEON code needs nothing beyond ARMv8
	return true;
#endif
}


status_t
VoiceInput::Start()
{
	{
		BAutolock _(fLock);
		fSamples.clear();
		fAccum = 0;
		fAccumCount = 0;
		fUntilNext = 0;
		fRMS = 0;
		fTalked = false;
		fAnyNoise = false;
		fQuietChunks = 0;
	}

	// As SoundRecorder does: a free output of the system's audio input,
	// recorded in whatever format it gives.
	status_t status;
	BMediaRoster* roster = BMediaRoster::Roster(&status);
	if (roster == NULL)
		return status;
	media_node input;
	if ((status = roster->GetAudioInput(&input)) != B_OK)
		return status;

	if (fRecorder == NULL) {
		fRecorder = new BMediaRecorder("Renku voice input", B_MEDIA_RAW_AUDIO);
		if ((status = fRecorder->InitCheck()) != B_OK)
			return status;
		media_format accepted;
		accepted.type = B_MEDIA_RAW_AUDIO;
		accepted.u.raw_audio = media_raw_audio_format::wildcard;
		fRecorder->SetAcceptedFormat(accepted);
	}

	media_output output;
	int32 count = 0;
	status = roster->GetFreeOutputsFor(input, &output, 1, &count,
		B_MEDIA_RAW_AUDIO);
	if (status != B_OK || count < 1)
		return status != B_OK ? status : B_BUSY;
	media_format format;
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio = output.format.u.raw_audio;

	fRecorder->SetHooks(_Record, NULL, this);
	if ((status = fRecorder->Connect(input, &output, &format)) != B_OK)
		return status;
	return fRecorder->Start();
}


void
VoiceInput::Stop()
{
	if (fRecorder == NULL || !fRecorder->IsConnected())
		return;
	fRecorder->Stop(true);
	fRecorder->Disconnect();
	fRecorder->SetHooks(NULL, NULL, NULL);
}


float
VoiceInput::Magnitude()
{
	BAutolock _(fLock);
	return 1.0f - powf(0.1f, 24.0f * fRMS);
}


VoiceInput::State
VoiceInput::CurrentState()
{
	BAutolock _(fLock);
	if (fTalked)
		return kTalking;
	if (!fAnyNoise && fSamples.size() > kBlockedAfter)
		return kMicMayBeBlocked;
	return kNotTalkedYet;
}


bool
VoiceInput::Finished()
{
	BAutolock _(fLock);
	return (fTalked && fQuietChunks >= kQuietChunksToStop)
		|| fSamples.size() >= kMaxSamples;
}


BString
VoiceInput::Transcribe()
{
	std::vector<float> samples;
	{
		BAutolock _(fLock);
		samples = fSamples;
	}
	if (samples.size() < kChunk)
		return "";
	save_recording(samples);

	if (fContext == NULL) {
		whisper_context_params contextParams
			= whisper_context_default_params();
		fContext = whisper_init_from_file_with_params(model_path().String(),
			contextParams);
		if (fContext == NULL)
			return "";
	}

	system_info info;
	get_system_info(&info);
	whisper_full_params params
		= whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	params.n_threads = std::clamp((int)info.cpu_count, 1, 4);
	params.language = "en";
	params.no_timestamps = true;
	params.single_segment = true;
	params.print_progress = false;
	params.print_realtime = false;
	params.print_timestamps = false;
	// ACFT: the encoder runs on the audio there is, 50 frames a second.
	params.audio_ctx = std::min(1500, (int)(samples.size() / 320) + 16);
	if (whisper_full(fContext, params, samples.data(), samples.size()) != 0)
		return "";

	BString text;
	for (int i = 0; i < whisper_full_n_segments(fContext); i++)
		text << whisper_full_get_segment_text(fContext, i);
	return text.Trim();
}


/*static*/ void
VoiceInput::_Record(void* cookie, bigtime_t timestamp, void* data,
	size_t size, const media_format& format)
{
	const media_raw_audio_format& raw = format.u.raw_audio;
	int32 channels = std::max((int32)raw.channel_count, (int32)1);
	size_t sampleSize = raw.format & media_raw_audio_format::B_AUDIO_SIZE_MASK;
	if (sampleSize == 0)
		return;
	size_t count = size / sampleSize;

	// to floats from -1 to 1, whatever the input gives
	std::vector<float> frames(count);
	for (size_t i = 0; i < count; i++) {
		const uint8* sample = (const uint8*)data + i * sampleSize;
		switch (raw.format) {
			case media_raw_audio_format::B_AUDIO_FLOAT:
				frames[i] = *(const float*)sample;
				break;
			case media_raw_audio_format::B_AUDIO_INT:
				frames[i] = *(const int32*)sample / 2147483648.0f;
				break;
			case media_raw_audio_format::B_AUDIO_SHORT:
				frames[i] = *(const int16*)sample / 32768.0f;
				break;
			case media_raw_audio_format::B_AUDIO_CHAR:
				frames[i] = *(const int8*)sample / 128.0f;
				break;
			case media_raw_audio_format::B_AUDIO_UCHAR:
				frames[i] = (*sample - 128) / 128.0f;
				break;
			default:
				return;
		}
	}
	((VoiceInput*)cookie)->_Take(frames.data(), count / channels, channels,
		raw.frame_rate > 0 ? raw.frame_rate : 48000);
}


/*!	Mixes down to mono and averages each 16 kHz sample's worth of frames
	into it, which also keeps out what's above 8 kHz.
*/
void
VoiceInput::_Take(const float* frames, size_t count, int32 channels,
	float rate)
{
	BAutolock _(fLock);
	double step = rate / kRate;
	for (size_t i = 0; i < count; i++) {
		float mono = 0;
		for (int32 c = 0; c < channels; c++)
			mono += frames[i * channels + c];
		fAccum += mono / channels;
		fAccumCount++;
		fUntilNext -= 1;
		if (fUntilNext > 0)
			continue;
		fUntilNext += step;
		if (fSamples.size() >= kMaxSamples)
			continue;
		fSamples.push_back(fAccum / fAccumCount);
		fAccum = 0;
		fAccumCount = 0;

		// each full 100 ms: how loud, and so whether there's speech
		if (fSamples.size() % kChunk != 0)
			continue;
		double sum = 0;
		for (size_t s = fSamples.size() - kChunk; s < fSamples.size(); s++)
			sum += fSamples[s] * fSamples[s];
		fRMS = sqrt(sum / kChunk);
		if (fRMS > kAnyNoiseRMS)
			fAnyNoise = true;
		bool speech = fSamples.size() > kIgnoreStart && fRMS > kSpeechRMS;
		if (speech)
			fTalked = true;
		fQuietChunks = speech ? 0 : fQuietChunks + 1;
	}
}
