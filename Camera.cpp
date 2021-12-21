#include <iostream>
#include "Camera.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <sys/mman.h>
#include <cstdint>
#include <poll.h>
#include <utility>
#include <cstddef>
#include <cerrno>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

// TODO: Add a function that converts general error codes into our error codes. That should be called when you're handling general error codes of every ioctl.

// TODO: Go through everything and check that your removal of bzero at some parts hasn't damaged setting reserved fields to zero, which is a requirement.

int interruptedIoctl(int fd, unsigned long request, void* argp) {
	int returnValue;
	do { returnValue = ioctl(fd, request, argp); }
	while (returnValue == -1 && errno == EINTR);
	return returnValue;
}

void* Camera::frameData() { return bufferLocations[0].start; }
size_t Camera::frameSize() { return bufferLocations[0].size; }

Camera::Camera(const char* deviceName) : deviceName(deviceName) {
	bzero(&buffers, sizeof(buffers));
	buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffers.memory = V4L2_MEMORY_MMAP;
}

Camera& Camera::operator=(Camera&& other) {
	deviceName = other.deviceName;
	fd = other.fd;
	format = other.format;
	buffers = other.buffers;
	bufferLocations = other.bufferLocations;

	initialized = false;
	other.fd = -1;

	return *this;
}

Camera::Camera(Camera&& other) { *this = std::move(other); }

CameraError Camera::open() {
	struct stat st;			// I think struct is necessary here because compiler interprets as stat() function otherwise.
	if (stat(deviceName, &st) == -1) { return CameraError::statusInfo; }
	if (!S_ISCHR(st.st_mode)) { return CameraError::fileNotDevice; }
	fd = ::open(deviceName, O_RDWR | O_NONBLOCK);
	if (fd == -1) { return CameraError::cannotOpen; }
	pollStruct.fd = fd;
	pollStruct.events = POLLIN;
	return CameraError::none;
}


CameraError Camera::readDeviceCapabilities() { if (interruptedIoctl(fd, VIDIOC_QUERYCAP, &capabilities) == -1) { return Error::device_capabilities_unavailable; } return Error::none; }

bool Camera::supportsVideoCapture() { return capabilities.cap & V4L2_CAP_VIDEO_CAPTURE; }
bool Camera::supportsStreaming() { return capabilities.cap & V4L2_CAP_STREAMING; }

CameraError Camera::readPreferredFormat() {
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_G_FMT, &format) == -1) { return Error::device_format_unavailable; }
	return Error::none;
}

Error Camera::tryFormat() {
	if (interruptedIoctl(fd, VIDIOC_TRY_FMT, &format) == -1) { return Error::device_format_unavailable; }
	return Error::none;
}

CameraError Camera::init(v4l2_format& format) {
	if (initialized) { return Error::not_freed; }			// don't allow initialization before freeing previous initialization

	// See if the camera supports capture and streaming. Necessary for capturing images through mmap.

	Error err = readDeviceCapabilities();
	if (err != Error::none) { return err; }

	if (!supportsVideoCapture()) { return Error::device_video_capture_unsupported; }
	if (!supportsStreaming()) { return Error::device_streaming_unsupported; }

	// negotiate device format

	if (interruptedIoctl(fd, VIDIOC_S_FMT, &format) == -1) { return Error::invalid_format_type; }
	if (format != ::format) { return Error::format_unsupported; }
	// If the check didn't fail, the format was set on the device, so we can continue.

	// initialize device buffers and set up shared memory access

	if (buffers.count == 0) { buffers.count = 1; }
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &buffers) == -1) { return Error::device_buffer_request_failed; }
	if (buffers.count == 0) { return Error::device_out_of_memory; }

	bufferLocations = (BufferLocation*)calloc(buffers.count, sizeof(BufferLocation));
	if (!bufferLocations) { err = CameraError::user_out_of_memory; goto freeDeviceBuffersAndReturnError; }

	int i = 0;				// TODO: This needs to be moved up because goto and stack.
	for (; i < buffers.count; i++) {
		struct v4l2_buffer buf;
		bzero(&buf, sizeof(buf));
		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (interruptedIoctl(fd, VIDIOC_QUERYBUF, &buf) == -1) { err = Error::device_buffer_query_failed; goto freeAndReturnError; }

		bufferLocations[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		if (bufferLocations[i].start == MAP_FAILED) { err = Error::mmap_failed; goto freeAndReturnError; }
		bufferLocations[i].size = buf.length;
	}

	initialized = true;
	return CameraError::none;

freeAndReturnError:
	// try to free the mmaps that didn't fail
	for (int j = 0; j < i; j++) { munmap(bufferLocations[j].start, bufferLocations[j].size); }	// no need to handle error here
	delete[] bufferLocations;
freeDeviceBuffersAndReturnError:
	buffers.count = 0;
	interruptedIoctl(fd, VIDIOC_REQBUFS, &buffers);
	return err;
}

CameraError Camera::init(uint32_t pixelFormat, uint32_t field) {
	Error err = readPreferredFormat();
	if (err != Error::none) { return err; }
	format.fmt.pix.pixelformat = pixelFormat;
	format.fmt.pix.field = field;
	return init(format);
}

CameraError Camera::init() { return init(V4L2_PIX_FMT_RGB24, V4L2_FIELD_NONE); }

Error Camera::readStreamingParameters() {
	bzero(&streamingParameters, sizeof(streamingParameters));
	streamingParameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_G_PARM, &streamingParameters) == -1) { return Error::device_streaming_parameters_unavailable; }
	return Error::none;
}

// NOTE: We would have to worry about the capture standard if we were supporting old devices, modern, digital devices (webcams, etc...) don't have v4l2 standards.
// NOTE: Capture standards set a minimum timePerFrame, which, following the above, we don't have to worry about here.
// NOTE: Device itself may limit (upper and lower) the timePerFrame, setting invalid values to their nearest valid values.
CameraError Camera::setTimePerFrame(uint32_t numerator, uint32_t denominator) {
	Error err = readStreamingParameters();
	if (err != Error::none) { return err; }
	if (streamingParameters.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
		streamingParameters.parm.capture.timeperframe.numerator = numerator;
		parmamingParameters.parm.capture.timeperframe.denominator = denominator;
		if (interruptedIoctl(fd, VIDIOC_S_PARM, &parm) == -1) { return Error::device_set_streaming_parameters_failed; }
		return Error::none;
	}
	return Error::device_custom_timeperframe_unsupported;
}

Error Camera::getTimePerFrame(uint32_t& numerator, uint32_t& denominator) {
	Error err = readStreamingParameters();
	if (err != Error::none) { return err; }
	numerator = streamingParameters.parm.capture.timeperframe.numerator;
	denominator = streamingParameters.parm.capture.timeperframe.denominator;
	return Error::none;
}

CameraError Camera::start() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_STREAMON, &type) == -1) { return CameraError::deviceStart; }
	return CameraError::none;
}

CameraError Camera::shootFrame() {
	struct v4l2_buffer buf;			// TODO: Does it make sense to keep creating new ones of these? Can this be optimized? If so, do that.
	bzero(&buf, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;			// Always use the first one for this.
	if (interruptedIoctl(fd, VIDIOC_QBUF, &buf) == -1) { return CameraError::deviceShootQBuf; }
	// TODO: Does buf.index = 0 being set here make a difference for DQBUF?
	if (poll(&pollStruct, 1, -1) == -1) { return CameraError::userShootPoll; }
	if (pollStruct.revents & POLLERR) { return CameraError::userShootPoll; }
	if (interruptedIoctl(fd, VIDIOC_DQBUF, &buf) == -1) { return CameraError::deviceShootDQBuf; }
	return CameraError::none;
}

CameraError Camera::stop() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_STREAMOFF, &type) == -1) { return CameraError::deviceStop; }
	return CameraError::none;
}

CameraError Camera::free() {
	if (buffers.count == 0) { return CameraError::none; }
	for (int i = 0; i < buffers.count; i++) { if (munmap(bufferLocations[i].start, bufferLocations[i].size) == -1) { return CameraError::mUnmapFailed; } }
	delete[] bufferLocations;
	buffers.count = 0;			// TODO: This whole setting count thing might be useless because driver cleans up after closing camera anyway, research.
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
