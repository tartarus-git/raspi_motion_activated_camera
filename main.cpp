#include <iostream>

#include "Camera.h"

int main() {
	std::cout << "testing camera..." << std::endl;
	Camera camera("/dev/video0");
	CameraError err = camera.open();
	if (err == CameraError::none) { std::cout << "camera was opened successfully" << std::endl; }
	else { std::cout << "issue while opening camera" << std::endl; }
	if ((err = camera.init()) == CameraError::none) { std::cout << "initialized successfully" << std::endl; }
	else { std::cout << "ERROR: while initializing." << std::endl << (int)err << std::endl;
		std::cout << "complying with device to make everything work despite incompatibility." << std::endl;
		err = camera.init(camera.format.fmt.pix.pixelformat, camera.format.fmt.pix.field);
		if (err != CameraError::none) { std::cout << "encountered error while doing that, all hope is lost" << std::endl; }
	}
	if (camera.start() == CameraError::none) { std::cout << "started successfully" << std::endl; }
	else { std::cout << "ERROR: while starting" << std::endl; }
	if (camera.shootFrame() == CameraError::none) { std::cout << "shot a frame successfully" << std::endl; }
	else { std::cout << "ERROR: while shooting" << std::endl; }
	std::cin.get();
	if (camera.stop() != CameraError::none) { std::cout << "ERROR: while stopping" << std::endl; }
	err = camera.close();
	if (err != CameraError::none) { std::cout << "camera was closed successfully" << std::endl; }
}
