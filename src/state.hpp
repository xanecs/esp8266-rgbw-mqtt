#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

class State {
	public:
		bool power;
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t w;
		uint8_t brightness;
		State(bool power, uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t brightness): power(power), r(r), g(g), b(b), w(w), brightness(brightness) {}
		State() : power(false), r(255), g(255), b(255), w(255), brightness(0) {}
};

#endif