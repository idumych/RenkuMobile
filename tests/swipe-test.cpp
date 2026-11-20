/*
 * swipe-test: the swipe decoder against made-up swipes, on the build host.
 * Each of the commonest words is swiped through its keys as the keyboard
 * lays them out, with each stop off centre and the corners cut, as a
 * finger does; the word has to come back first, or among three.
 */
#include "../shell/src/SwipeDecoder.h"
#include "../shell/src/WordData.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

	// Through the key centres, a word beats a commoner one ending a key away.
	std::vector<SwipePoint> hello = { keys['h' - 'a'], keys['e' - 'a'],
		keys['l' - 'a'], keys['o' - 'a'] };
	assert(strcmp(decoder.Decode(hello, 1)[0], "hello") == 0);

	printf("swipe decoder: %d words, %.1f%% first, %.1f%% in the top three\n",
		tried, 100.0 * first / tried, 100.0 * three / tried);
	assert(first >= tried * 0.75 && three >= tried * 0.90);
	return 0;
}
