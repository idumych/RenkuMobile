/*
 * SwipeDecoder: the words a finger's path across the letters could mean.
 *
 * Our own shape matching, not FUTO's (whose decoder is neural, under its own
 * licence): every word's path from key centre to key centre, its corners
 * cut as a finger cuts them, is compared with the finger's, both resampled to the same number of points, and how
 * far apart they lie is weighed against how common the word is. Only words
 * that start and end near where the finger did are tried.
 *
 * It knows nothing of the Interface Kit, so tests/swipe-test.cpp can check
 * it on the build host.
 */
#ifndef SWIPE_DECODER_H
#define SWIPE_DECODER_H

#include <vector>


struct SwipePoint {
	float	x;
	float	y;
};


class SwipeDecoder {
public:
								SwipeDecoder();

			// Where each letter's key is, and how wide keys are.
			void				SetKey(char letter, SwipePoint center);
			void				SetKeyWidth(float width);

			// The likeliest words, best first.
			std::vector<const char*> Decode(
									const std::vector<SwipePoint>& path,
									size_t count) const;

private:
			SwipePoint			fKeys[26];
			bool				fHasKey[26];
			float				fKeyWidth;
			// Each word's finger path, worked out when first needed.
	mutable	std::vector<std::vector<SwipePoint>> fPaths;
};


#endif	// SWIPE_DECODER_H
