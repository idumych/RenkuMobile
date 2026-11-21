/*
 * swipe-test: the swipe decoder against made-up swipes, on the build host.
 * Each of the commonest words is swiped through its keys as the keyboard
 * lays them out, with each stop off centre and the corners cut, as a
 * finger does; the word has to come back first, or among three. Then the
 * words of seven letters or more are swiped again with the corners cut as
 * a quick finger cuts them, often missing the keys it turns at.
 */
#include "../shell/src/SwipeDecoder.h"
#include "../shell/src/WordData.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <math.h>
#include <random>


int
main()
{
	// The keyboard's letter keys at 720 pixels wide (Keyboard.cpp's _Layout).
	const float unit = (720 - 8 * 11) / 10.0f, pitch = unit + 8;
	const char* rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
	const float firstCenter[] = { 8 + unit / 2, 43.6f + unit / 2,
		12 + unit * 1.5f + 8 + unit / 2 };
	SwipeDecoder decoder;
	decoder.SetKeyWidth(unit);
	SwipePoint keys[26];
	for (int row = 0; row < 3; row++) {
		for (int i = 0; rows[row][i] != '\0'; i++) {
			SwipePoint center = { firstCenter[row] + i * pitch, 36 + row * 80.0f };
			decoder.SetKey(rows[row][i], center);
			keys[rows[row][i] - 'a'] = center;
		}
	}

	std::mt19937 random(1);
	std::normal_distribution<float> offCentre(0, unit * 0.2f);
	int tried = 0, first = 0, three = 0;
	for (int w = 0; w < kSwipeWordCount && tried < 3000; w++) {
		std::vector<SwipePoint> stops;
		char last = 0;
		for (const char* c = kSwipeWords[w].word; *c != '\0'; c++) {
			char letter = tolower(*c);
			if (letter == '\'' || letter == last)
				continue;
			SwipePoint key = keys[letter - 'a'];
			stops.push_back({ key.x + offCentre(random),
				key.y + offCentre(random) });
			last = letter;
		}
		if (stops.size() < 2)
			continue;

		// straight between the stops, then the corners rounded off twice
		std::vector<SwipePoint> path;
		for (size_t i = 0; i + 1 < stops.size(); i++) {
			for (int s = 0; s < 6; s++) {
				float t = s / 6.0f;
				path.push_back({ stops[i].x + (stops[i + 1].x - stops[i].x) * t,
					stops[i].y + (stops[i + 1].y - stops[i].y) * t });
			}
		}
		path.push_back(stops.back());
		for (int pass = 0; pass < 2; pass++) {
			std::vector<SwipePoint> round = { path.front() };
			for (size_t i = 0; i + 1 < path.size(); i++) {
				SwipePoint a = path[i], b = path[i + 1];
				round.push_back({ a.x * 0.75f + b.x * 0.25f, a.y * 0.75f + b.y * 0.25f });
				round.push_back({ a.x * 0.25f + b.x * 0.75f, a.y * 0.25f + b.y * 0.75f });
			}
			round.push_back(path.back());
			path = round;
		}

		std::vector<const char*> words = decoder.Decode(path, 3);
		tried++;
		for (size_t i = 0; i < words.size(); i++) {
			if (words[i] == kSwipeWords[w].word) {
				first += i == 0;
				three++;
			}
		}
	}

	// Long words, the path rounded off by a gaussian a key width wide.
	int longTried = 0, longFirst = 0;
	std::normal_distribution<float> sloppy(0, unit * 0.3f);
	for (int w = 0; w < kSwipeWordCount && longTried < 1000; w++) {
		if (strlen(kSwipeWords[w].word) < 7)
			continue;
		std::vector<SwipePoint> dense;
		SwipePoint previous = { 0, 0 };
		char last = 0;
		for (const char* c = kSwipeWords[w].word; *c != '\0'; c++) {
			char letter = tolower(*c);
			if (letter == '\'' || letter == last)
				continue;
			SwipePoint key = keys[letter - 'a'];
			SwipePoint stop = { key.x + sloppy(random), key.y + sloppy(random) };
			if (!dense.empty()) {
				// 2 px apart
				int steps = std::max(1, (int)(hypotf(stop.x - previous.x,
					stop.y - previous.y) / 2));
				for (int s = 1; s <= steps; s++) {
					float t = (float)s / steps;
					dense.push_back({ previous.x + (stop.x - previous.x) * t,
						previous.y + (stop.y - previous.y) * t });
				}
			} else
				dense.push_back(stop);
			previous = stop;
			last = letter;
		}
		const float sigma = unit / 2;
		const int radius = (int)(sigma * 3 / 2), count = dense.size();
		std::vector<SwipePoint> path;
		for (int i = 0; i < count; i += 3) {
			float x = 0, y = 0, total = 0;
			for (int j = -radius; j <= radius; j++) {
				SwipePoint point = dense[std::clamp(i + j, 0, count - 1)];
				float weight = expf(-(j * 2) * (j * 2) / (2 * sigma * sigma));
				x += point.x * weight;
				y += point.y * weight;
				total += weight;
			}
			path.push_back({ x / total, y / total });
		}

		std::vector<const char*> words = decoder.Decode(path, 1);
		longTried++;
		longFirst += !words.empty() && words[0] == kSwipeWords[w].word;
	}

	// Through the key centres, a word beats a commoner one ending a key away.
	std::vector<SwipePoint> hello = { keys['h' - 'a'], keys['e' - 'a'],
		keys['l' - 'a'], keys['o' - 'a'] };
	assert(strcmp(decoder.Decode(hello, 1)[0], "hello") == 0);

	printf("swipe decoder: %d words, %.1f%% first, %.1f%% in the top three\n",
		tried, 100.0 * first / tried, 100.0 * three / tried);
	printf("swipe decoder: %d long words swiped roughly, %.1f%% first\n",
		longTried, 100.0 * longFirst / longTried);
	assert(first >= tried * 0.75 && three >= tried * 0.90);
	assert(longFirst >= longTried * 0.8);
	return 0;
}
