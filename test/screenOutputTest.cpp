#define X_PADDING 50
#define Y_PADDING 50

#include "../include/Screen.h"

#include <signal.h>
#include <iostream>
#include <cstdlib>
#include <cmath>

bool isAlive = true;

void signalHandler(int signum) { isAlive = false; }

int main() {
	signal(SIGINT, signalHandler);
	std::cout << "testing screen output" << std::endl;
	vid::Screen screen;
	vid::Screen::Error err = screen.open();
	if (err != vid::Screen::Error::none) {
		std::cout << "error encountered while opening screen, err: " << err << std::endl;
		return 0;
	}
	err = screen.init();
	if (err != vid::Screen::Error::none) {
		std::cout << "error encountered while initializing screen, err: " << err << std::endl;
		return 0;
	}
	std::cout << "data:" << std::endl;
	std::cout << "width: " << screen.width() << ", height: " << screen.height() << std::endl;
	std::cout << "bpp: " << screen.variableInfo.bits_per_pixel << std::endl;
	for (unsigned int y = 0; y < screen.height(); y++) {
		for (unsigned int x = 0; x < X_PADDING; x++) {
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 0] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 1] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 2] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 3] = rand() % 256;
		}
		for (unsigned int x = screen.width() - X_PADDING; x < screen.width(); x++) {
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 0] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 1] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 2] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 3] = 255;
		}
	}
	for (unsigned int y = 0; y < Y_PADDING; y++) {
		for (unsigned int x = 0; x < screen.width(); x++) {
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 0] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 1] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 2] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 3] = 255;
		}
	}
	for (unsigned int y = screen.height() - Y_PADDING; y < screen.height(); y++) {
		for (unsigned int x = 0; x < screen.width(); x++) {
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 0] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 1] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 2] = rand() % 256;
			((unsigned char*)screen.frame)[y * screen.width() * 4 + x * 4 + 3] = 255;
		}
	}
	unsigned char color = 255;
	unsigned int xPos = 300;
	unsigned int yPos = 300;
	while (isAlive) {
		for (int y = Y_PADDING; y < screen.height() - Y_PADDING; y++) {
			for (int x = X_PADDING * 4; x < 4 * (screen.width() - X_PADDING); x++) {
				if (sqrt((double)((xPos - x / 4) * (xPos - x / 4) + (yPos - y) * (yPos - y))) > 100) { continue; }
				((unsigned char*)screen.frame)[y * screen.width() * 4 + x] = color;
			}
		}
	}
	std::cout << "exiting test..." << std::endl;
	err = screen.free();
	if (err != vid::Screen::Error::none) {
		std::cout << "error encountered while freeing screen, err: " << err << std::endl;
		return 0;
	}
	err = screen.close();
	if (err != vid::Screen::Error::none) {
		std::cout << "error encountered while closing screen, err: " << err << std::endl;
		return 0;
	}
}
