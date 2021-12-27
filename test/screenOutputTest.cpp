#include "../include/Screen.h"

#include <iostream>

int main() {
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
	for (size_t i = 0; i < screen.frameSize; i++) { ((char*)screen.frame)[i] = 255; }
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
