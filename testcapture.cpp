#include <iostream>
#include <opencv2/opencv.hpp>			// Using opencv for saving videos because I have it on hand at the moment, gonna look for a better solution.
#include "Camera.h"

int main() {
	Camera camera("/dev/video0");
	Camera::Error err = camera.open(); if (err != Camera::Error::none) { std::cout << "something went wrong while opening camera" << std::endl; }
	camera.bufferMetadata.count = 30;
	err = camera.readPreferredFormat(); if (err != Camera::Error::none) { std::cout << "something went wrong reading format" << std::endl; return 0; }
	std::cout << camera.format.fmt.pix.pixelformat << std::endl;
	std::cout << camera.format.fmt.pix.field << std::endl;
	camera.stop();
	err = camera.init(); if (err != Camera::Error::none) { std::cout << "something went wrong while initializating camera, err: " << err << " errno: " << errno << std::endl; }
	std::cout << camera.format.fmt.pix.pixelformat << std::endl;
	std::cout << camera.format.fmt.pix.field << std::endl;
	err = camera.setTimePerFrame(1, 30); if (err != Camera::Error::none) { std::cout << "something went wrong while setting FPS" << std::endl; }
	uint32_t num;
	uint32_t denum;
	err = camera.getTimePerFrame(num, denum); if (err) { std::cout << "couldn't get fps" << std::endl; }
	std::cout << "numerator: " << num << "denominator: " << denum << std::endl;
	err = camera.start(); if (err != Camera::Error::none) { std::cout << "something went wrong while starting camera stream" << std::endl; }

	cv::VideoWriter output = cv::VideoWriter("output.avi", cv::VideoWriter::fourcc('X', '2', '6', '4'), 30, cv::Size(camera.format.fmt.pix.width, camera.format.fmt.pix.height));
	if (!output.isOpened()) { std::cout << "OpenCV video writer didn't open right" << std::endl; return 0; }

	err = camera.queueAllFrames(); if (err != Camera::Error::none) { std::cout << "couldn't queue all frames" << std::endl; return 0; }
	cv::Mat mat;
	while (true) {
		err = camera.dequeueFrame(); if (err != Camera::Error::none) { std::cout << "couldn't dequeue frame" << std::endl; return 0; }
		// TODO: Write frame.
		err = camera.queueFrame(); if (err != Camera::Error::none) { std::cout << "couldn't queue frame" << std::endl; return 0; }
	}
}
