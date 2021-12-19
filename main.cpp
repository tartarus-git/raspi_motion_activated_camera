#include <iostream>

#include "Camera.h"

int main() {
	std::cout << "testing camera..." << std::endl;
	Camera camera("/dev/video0");
	CameraError err = camera.open();
	if (err == CameraError::none) { std::cout << "camera was opened successfully" << std::endl; }
	else { std::cout << "issue while opening camera" << std::endl; }
	std::cin.get();
	err = camera.close();
	if (err == CameraError::none) { std::cout << "camera was closed successfully" << std::endl; }
}
