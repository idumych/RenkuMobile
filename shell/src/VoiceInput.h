/*
 * VoiceInput: dictation, after FUTO's voice input. It records the default
 * audio input at 16 kHz mono and turns what was said into text with
 * whisper.cpp and FUTO's ACFT tiny.en model, whose encoder is run on only as
 * much audio as there is (audio_ctx) rather than on 30 seconds.
 *
 * As in FUTO's, it watches the loudness: speech once it rises above 0.01
 * (ignoring the first 0.6 s), done after 2 s of quiet that follows speech,
 * and a microphone that may be blocked when nothing at all is heard for 2 s.
 */
#ifndef VOICE_INPUT_H
#define VOICE_INPUT_H

#include <Locker.h>
#include <MediaDefs.h>
#include <String.h>

#include <vector>


class BMediaRecorder;
struct whisper_context;


class VoiceInput {
public:
	enum State { kNotTalkedYet, kTalking, kMicMayBeBlocked };

								VoiceInput();
								~VoiceInput();

			// Whether dictation can work here: the model is installed and
			// the CPU runs the build (AVX2).
	static	bool				Available();

			status_t			Start();
			void				Stop();

			// For the listening view: how loud, from 0 to 1, as FUTO scales
			// it; whether speech was heard; and whether to stop by itself.
			float				Magnitude();
			State				CurrentState();
			bool				Finished();

			// What was said. Slow: call it away from any window's thread.
			BString				Transcribe();

private:
	static	void				_Record(void* cookie, bigtime_t timestamp,
									void* data, size_t size,
									const media_format& format);
			void				_Take(const float* frames, size_t count,
									int32 channels, float rate);

			BMediaRecorder*		fRecorder;
			BLocker				fLock;
			std::vector<float>	fSamples;	// 16 kHz mono
			double				fAccum;
			int32				fAccumCount;
			double				fUntilNext;	// input frames to the next sample
			float				fRMS;
			bool				fTalked;
			bool				fAnyNoise;
			int32				fQuietChunks;
			whisper_context*	fContext;
};


#endif	// VOICE_INPUT_H
