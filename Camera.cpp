#include "Camera.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <sys/mman.h>
#include <cstdint>

#include <linux/videodev2.h>

void interruptedIoctl(int fd, unsigned long request, void* argp) {
	int returnValue;
	do { returnValue = ioctl(fd, request, argp); }
	while (returnValue == -1 && errno == EINTR);
	return returnValue;
}

Camera::Camera(const char* deviceName) : deviceName(deviceName) {
	bzero(*buffers, sizeof(buffers));
	buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffers.count = 0;
	buffers.memory = V4L2_MEMORY_MMAP;
}

Camera::Camera(Camera&& other) {
	deviceName = other.deviceName;
	fd = other.fd;
	format = other.format;
	buffers = other.buffers;
	bufferLocations = other.bufferLocations;

	other.buffers.count = 0;
	other.fd = -1;
}

Camera& Camera::operator=(Camera&& other) { this->Camera(std::move(other)); return *this; }

CameraError Camera::open() {
	struct stat st;			// I think struct is necessary here because compiler interprets as stat() function otherwise.
	if (stat(deviceName, &st) == -1) { return CameraError::statusInfo; }
	if (!S_ISCHR(st.st_mode)) { return CameraError::fileNotDevice; }
	fd = ::open(deviceName, O_RDWR | O_NONBLOCK);
	if (fd == -1) { return CameraError::cannotOpen; }
	return CameraError::none;
}


CameraError Camera::init() {
	struct v4l2_capability cap;			// This has struct because tells reader that v4l2_* are structs and not other types. Design choice.

	// Get device capability information and device format information.

	if (interruptedIoctl(fd, VIDIOC_QUERYCAP, &cap) == -1) { return CameraError::deviceQueryCap; }	// Get capability information from device.
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) { return CameraError::deviceNoVideo; }	// See if device supports video.
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) { return CameraError::deviceNoStreaming; }	// See if device supports streaming with shared memory.
	// NOTE: using mmap or userptr with V4L2 is supposed to be way faster than doing reads and writes to the dev file, which is why we're using mmap.
	
	bzero(*format, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_G_FMT, &format) == -1) { return CameraError::deviceGetFormat; }

	// Initialize shared memory.
	
	buffers.count = 1;				// Try to use just a single buffer because that works better for real-time stuff.
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &buffers) == -1) { if (errno == EINVAL) { return CameraError::deviceNoMMap; } return CameraError::deviceRequestBuffer; }
	if (buffers.count == 0) { return CameraError::deviceInsufficientMemory; }
	bufferLocations = (BufferLocation*)calloc(buffers.count, sizeof(BufferLocation));
	if (!bufferLocations) { return CameraError::userInsufficientMemory; }
	for (int i = 0; i < buffers.count; i++) {
		struct v4l2_buffer buf;
		bzero(*buf, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		
		if (interruptedIoctl(fd, VIDIOC_QUERYBUF, &buf) == -1) { return CameraError::deviceQueryBuf; }

		bufferLocations[i].size = buf.length;
		bufferLocations[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		if (bufferLocations[i].start == MAP_FAILED) { return CameraError::mMapFailed; }
	}

	return CameraError::none;
}

// NOTE: We would have to worry about the capture standard if we weren't using a modern, digital camera. Since we are, we can set the FPS to whatever we want.
CameraError Camera::setFPS(uint32_t FPS) {
	struct v4l2_streamparm parm;
	bzero(*parm, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIO_G_PARM, &parm) == -1) { return CameraError::deviceGetParm; }
	if (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
		parm.parm.capture.timeperframe.numerator = 1;
		parm.parm.capture.timeperframe.denominator = FPS;
		if (interruptedIoctl(fd, VIDIO_S_PARM, &parm) == -1) { return CameraError::deviceSetParm; }
		return CameraError::none;
	}
	return CameraError::deviceNoCustomFPS;
}

uint32_t Camera::getFPS(CameraError& error) {
	struct v4l2_streamparm parm;
	bzero(*parm, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIO_G_PARM, &parm) == -1) { error = CameraError::deviceGetParm; return 0; }
	error = CameraError::none;
	return parm.capture.timeperframe.denominator / parm.capture.timeperframe.numerator;
}

uint32_t Camera::getFPS() { CameraError error; return getFPS(error); }

CameraError Camera::free() {
	if (buffers.count == 0) { return CameraError::none; }
	delete[] bufferLocations;
	buffers.count = 0;
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &buffers) == -1) { if (errno == EINVAL) { return CameraError::deviceNoMMap; } return CameraError::deviceRequestBuffer; }
	if (buffers.count != 0) { return CameraError::deviceMemFree; }
	return CameraError::none;
}

CameraError Camera::close() {
	if (fd == -1) { return CameraError::cannotClose; }
	free();
	if (::close(fd) == -1) { fd = -1; return CameraError::cannotClose; }
	fd = -1;
	return CameraError::none;
}

Camera::~Camera() { close(); }
