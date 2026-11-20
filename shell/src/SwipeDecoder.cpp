/*
 * SwipeDecoder: see SwipeDecoder.h.
 */
#include "SwipeDecoder.h"

#include "WordData.h"

#include <algorithm>
#include <ctype.h>
#include <math.h>


static const int kSamples = 32;
// A word must start and end within this many key widths of the finger.
static const float kEndSlop = 1.2f;
// How fast the match falls off with distance, in key widths.
static const float kSigma = 0.35f;
// How much a word's commonness counts against its distance.
static const float kWeightScale = 0.006f;


static float
distance(SwipePoint a, SwipePoint b)
{
	return hypotf(a.x - b.x, a.y - b.y);
}


/*!	\a count points spread evenly along the polyline. */
static std::vector<SwipePoint>
resample(const std::vector<SwipePoint>& points, int count)
{
	float length = 0;
	for (size_t i = 1; i < points.size(); i++)
		length += distance(points[i - 1], points[i]);

	std::vector<SwipePoint> out;
	if (length == 0) {
		out.assign(count, points[0]);
		return out;
	}

	float step = length / (count - 1);
	float travelled = 0;
	size_t segment = 1;
	for (int i = 0; i < count; i++) {
		float target = std::min(i * step, length);
		while (segment < points.size() - 1
			&& travelled + distance(points[segment - 1], points[segment])
				< target) {
			travelled += distance(points[segment - 1], points[segment]);
			segment++;
		}
		SwipePoint a = points[segment - 1], b = points[segment];
		float span = distance(a, b);
		float t = span > 0 ? std::min(1.0f, (target - travelled) / span) : 0;
		out.push_back({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t });
	}
	return out;
}


SwipeDecoder::SwipeDecoder()
	:
	fKeyWidth(1)
{
	std::fill(fHasKey, fHasKey + 26, false);
}


void
SwipeDecoder::SetKey(char letter, SwipePoint center)
{
	int index = tolower(letter) - 'a';
	if (index < 0 || index >= 26)
		return;
	fKeys[index] = center;
	fHasKey[index] = true;
}


void
SwipeDecoder::SetKeyWidth(float width)
{
	fKeyWidth = width;
}


std::vector<const char*>
SwipeDecoder::Decode(const std::vector<SwipePoint>& path, size_t count) const
{
	std::vector<std::pair<float, const char*>> scored;
	if (path.size() < 2)
		return {};
	std::vector<SwipePoint> swipe = resample(path, kSamples);
	float slop = kEndSlop * fKeyWidth;

	std::vector<SwipePoint> keys;
	for (int w = 0; w < kSwipeWordCount; w++) {
		// The word's keys, a letter typed twice being one stop.
		keys.clear();
		bool known = true;
		char last = 0;
		for (const char* c = kSwipeWords[w].word; *c != '\0'; c++) {
			char letter = tolower(*c);
			if (letter == '\'' || letter == last)
				continue;
			int index = letter - 'a';
			if (index < 0 || index >= 26 || !fHasKey[index]) {
				known = false;
				break;
			}
			keys.push_back(fKeys[index]);
			last = letter;
		}
		// A one-key word is a tap, not a swipe.
		if (!known || keys.size() < 2
			|| distance(keys.front(), swipe.front()) > slop
			|| distance(keys.back(), swipe.back()) > slop) {
			continue;
		}

		// How far the word's path lies from the finger's, on average, and
		// how near the finger came to the key it passed farthest from: a
		// key never reached counts in full, not shared out among the rest.
		std::vector<SwipePoint> ideal = resample(keys, kSamples);
		float apart = 0;
		for (int i = 0; i < kSamples; i++)
			apart += distance(ideal[i], swipe[i]);
		apart /= kSamples * fKeyWidth;

		float missed = 0;
		for (SwipePoint key : keys) {
			float nearest = INFINITY;
			for (SwipePoint point : swipe)
				nearest = std::min(nearest, distance(key, point));
			missed = std::max(missed, nearest);
		}
		missed /= fKeyWidth;

		float cost = apart + missed;
		scored.push_back({ -cost * cost / (2 * kSigma * kSigma)
			+ kWeightScale * kSwipeWords[w].weight, kSwipeWords[w].word });
	}

	count = std::min(count, scored.size());
	std::partial_sort(scored.begin(), scored.begin() + count, scored.end(),
		[](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<const char*> words;
	for (size_t i = 0; i < count; i++)
		words.push_back(scored[i].second);
	return words;
}
