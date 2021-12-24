#include <iostream>
#include <chrono>
#include <ratio>

#include "Camera.h"

#include <linux/videodev2.h>

int main() {

	// Tests for the syntax and type conversion stuff of Camera::Error.
	//Camera::Error testError = 10;		<-- doesn't work, this is good
	Camera::Error testError = Camera::Error::status_info_unavailable;
	int testErrorIntVersion = testError;
	bool testErrorBoolVersion = testError;
	long long int testErrorlonglongversion = testError;
	Camera::Error testError2(Camera::Error::status_info_unavailable);
	Camera::Error testError3(Camera::Error(Camera::Error::status_info_unavailable));
	testError = Camera::Error::ErrorValue::status_info_unavailable;
	Camera::Error::ErrorValue errorvaltestthing = Camera::Error::status_info_unavailable;



	std::cout << "starting camera test..." << std::endl;
	Camera camera("/dev/video0");
	Camera::Error err = camera.open();
	if (err == Camera::Error::none) { std::cout << "constructed and opened camera successfully" << std::endl; }
	else { std::cout << "open() failed with error code: " << (int)err << std::endl; }
	std::cout << "press enter to try setting FPS before initializing camera" << std::endl;
	std::cin.get();
	err = camera.setTimePerFrame(1, 5);
	if (err == Camera::Error::none) { std::cout << "setting FPS went fine" << std::endl; }
	else { std::cout << "error while settings FPS before init()" << std::endl; }
	std::cout << "press enter to initialize camera" << std::endl;
	std::cin.get();
	camera.readPreferredFormat();
	err = camera.init();
	if (err == Camera::Error::none) { std::cout << "initialization of camera was successful" << std::endl; }
	else { std::cout << "initialization failed with error code: " << (int)err << std::endl; }
	std::cout << "amount of buffers: " << camera.bufferMetadata.count << std::endl;
	std::cout << "press enter to set FPS to 5 (potentially again, if the one before worked)" << std::endl;
	std::cin.get();
	err = camera.setTimePerFrame(1, 5);
	if (err == Camera::Error::none) { std::cout << "setting FPS went fine" << std::endl; }
	else { std::cout << "error while setting FPS after init()" << std::endl; }

	std::cout << "because no alterations were made, camera is currently working with one frame buffer" << std::endl;
	std::cout << "press enter to try queueing and dequeueing a frame before starting the stream" << std::endl;
	std::cin.get();
	err = camera.queueFrame();
	if (err == Camera::Error::none) { std::cout << "queueing frame before start() went fine" << std::endl; }
	else { std::cout << "queueing frame before start() had problem: " << (int)err << std::endl; }
	err = camera.dequeueFrame();
	if (err == Camera::Error::none) { std::cout << "dequeueing went fine" << std::endl; }
	else if (err == Camera::Error::dequeue_frame_impossible) { std::cout << "dequeue threw dequeue_frame_impossible back" << std::endl; }
	else { std::cout << "dequeue failed for some other reason, err: " << (int)err << std::endl; }

	if (camera.stop()) { std::cout << "stopping the stream to get rid of queued frame resulted in an issue" << std::endl; }

	std::cout << "press enter to test if dequeueFrame returns dequeue_frame_impossible when called before queueing anything but after start()" << std::endl;
	std::cin.get();
	if (camera.start()) { std::cout << "had issues starting stream" << std::endl; }
	err = camera.dequeueFrame();
	if (err == Camera::Error::none) { std::cout << "had no issues dequeueing nothing" << std::endl; }
	else if (err == Camera::Error::dequeue_frame_impossible) { std::cout << "dequeue_frame_impossible encountered" << std::endl; }
	else { std::cout << "other error on dequeueFrame, err: " << (int)err << std::endl; }

	std::cout << "press enter to shoot 5 frames as FPS of 5 and return the time that it took to shoot them" << std::endl;
	std::cin.get();

	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 5; i++) {
		std::cout << "shooting frame..." << std::endl;
		if (err = camera.shootFrame()) { std::cout << "had problems shooting the frame, err: " << err << std::endl; }
	}
	std::chrono::duration<double, std::ratio<1>> duration = std::chrono::high_resolution_clock::now() - start;
	std::cout << "took " << duration.count() << " seconds to capture 5 frames" << std::endl;

	std::cout << "press enter to test if calling stop() and then calling dequeueFrame() results in dequeue_frame_impossible. It might not since we already started and stopped the stream and the documentation for v4l2 is kind of bad so I don't know what to expect." << std::endl;
	std::cin.get();
	if (camera.stop()) { std::cout << "had a problem stopping stream" << std::endl; }
	err = camera.dequeueFrame();
	if (err == Camera::Error::none) { std::cout << "had no issues dequeueing a frame" << std::endl; }
	else if (err == Camera::Error::dequeue_frame_impossible) { std::cout << "dequeue failed with dequeue_frame_impossible" << std::endl; }
	else { std::cout << "dequeueFrame() failed with some other error, err: " << err << std::endl; }

	std::cout << "press enter to dispose all resources and clean up everything, then quit" << std::endl;
	std::cin.get();
	if (camera.close() != Camera::Error::none) { std::cout << "problem while cleaning up" << std::endl; }
	else { std::cout << "clean up went fine, quitting..." << std::endl; }
}
