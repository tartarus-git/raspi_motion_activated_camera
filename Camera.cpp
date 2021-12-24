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
#include <cstring>

#include <linux/videodev2.h>

// Camera::Error

Camera::Error::Error(Camera::Error::ErrorValue value) noexcept : value(value) { }

Camera::Error::operator int() const noexcept { return value; }

// Camera

// SIDE-NOTE: Can you modify a string literal? They're expressed as const char*, but (through casting to char*) can you modify them?
// Answer: No, you can't. It would work out syntactically and the compiler would let you, but it isn't possible because string literals are stored in read-only memory.
// Accessing them, even if C++ lets you, is undefined behaviour. I can imagine that it will probably end in a seg fault.
// Fun fact: char*="x" is also a pointer to string literal, so changing any part of that results in undefined behaviour, even though the language very easily lets you.

// Does the same thing as ioctl, but recovers from signals interrupting the syscall by running the syscall over and over until it doesn't report an interrupt.
// "over and over" will basically never happen. If it doesn't succeed on the first try, it probably will on the second.
int interruptedIoctl(int fd, unsigned long request, void* argp) {
	int returnValue;
	do { returnValue = ioctl(fd, request, argp); }
	while (returnValue == -1 && errno == EINTR);
	return returnValue;
}

int interruptedPoll(struct pollfd *fds, nfds_t nfds, int timeout) {
	int returnValue;
	do { returnValue = poll(fds, nfds, timeout); }
	while (returnValue == -1 && errno == EINTR);
	return returnValue;
}

Camera::Camera(const char* deviceName) : deviceName(deviceName) {
	bzero(&bufferMetadata, sizeof(bufferMetadata));
	bufferMetadata.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferMetadata.memory = V4L2_MEMORY_MMAP;

	bzero(&bufferData, sizeof(bufferData));
	bufferData.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferData.memory = V4L2_MEMORY_MMAP;

	pollStruct.events = POLLIN;
}

Camera& Camera::operator=(Camera&& other) {
	deviceName = other.deviceName;
	fd = other.fd;
	capabilities = other.capabilities;
	format = other.format;
	bufferMetadata = other.bufferMetadata;
	lastBufferIndex = other.lastBufferIndex;
	bufferData = other.bufferData;
	streamingParameters = other.streamingParameters;

	frameLocations = other.frameLocations;

	other.initialized = false;
	other.fd = -1;

	return *this;
}

Camera::Camera(Camera&& other) { *this = std::move(other); }

Camera::Error Camera::open() {
	struct stat st;					// I think struct is necessary here because compiler interprets as stat() function otherwise.
	if (stat(deviceName, &st) == -1) { return Error::status_info_unavailable; }
	if (!S_ISCHR(st.st_mode)) { return Error::file_is_not_device; }

	fd = ::open(deviceName, O_RDWR | O_NONBLOCK);
	if (fd == -1) { return Error::file_open_failed; }

	pollStruct.fd = fd;

	return Error::none;
}


Camera::Error Camera::readCapabilities() { if (interruptedIoctl(fd, VIDIOC_QUERYCAP, &capabilities) == -1) { return Error::device_capabilities_unavailable; } return Error::none; }

bool Camera::supportsVideoCapture() { return capabilities.capabilities & V4L2_CAP_VIDEO_CAPTURE; }
bool Camera::supportsStreaming() { return capabilities.capabilities & V4L2_CAP_STREAMING; }

Camera::Error Camera::readPreferredFormat() {
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_G_FMT, &format) == -1) { return Error::device_format_unavailable; }
	return Error::none;
}

Camera::Error Camera::tryFormat() { if (interruptedIoctl(fd, VIDIOC_TRY_FMT, &format) == -1) { return Error::device_format_unavailable; } return Error::none; }

Camera::Error Camera::init() {
	if (initialized) { return Error::not_freed; }			// don't allow initialization before freeing previous initialization

	// See if the camera supports capture and streaming. Necessary for getting images through mmap.

	Error err = readCapabilities();
	if (err != Error::none) { return err; }

	if (!supportsVideoCapture()) { return Error::device_video_capture_unsupported; }
	if (!supportsStreaming()) { return Error::device_streaming_unsupported; }

	// negotiate device format

	v4l2_format previousFormatState = format;
	if (interruptedIoctl(fd, VIDIOC_S_FMT, &format) == -1) { return Error::invalid_format_type; }
	if (memcmp(&format, &previousFormatState, sizeof(v4l2_format)) != 0) { return Error::format_unsupported; }
	// If the check didn't fail, the format was set on the device, so we can continue.

	// initialize device buffers and user data and set up shared memory access

	if (bufferMetadata.count == 0) { bufferMetadata.count = 1; }
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &bufferMetadata) == -1) { return Error::device_buffer_request_failed; }
	if (bufferMetadata.count == 0) { return Error::device_out_of_memory; }
	lastBufferIndex = bufferMetadata.count - 1;

	frameLocations = (BufferLocation*)calloc(bufferMetadata.count, sizeof(BufferLocation));
	if (!frameLocations) { err = Error::user_out_of_memory; goto freeDeviceBuffersAndReturnError; }

	// NOTE: The first check that the for loop does is useless, I assume it'll get optimized out.
	for (bufferData.index = 0; bufferData.index < bufferMetadata.count; bufferData.index++) {
		if (interruptedIoctl(fd, VIDIOC_QUERYBUF, &bufferData) == -1) { err = Error::device_buffer_query_failed; goto freeAndReturnError; }
		frameLocations[bufferData.index].start = mmap(nullptr, bufferData.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bufferData.m.offset);
		if (frameLocations[bufferData.index].start == MAP_FAILED) { err = Error::mmap_failed; goto freeAndReturnError; }
		frameLocations[bufferData.index].size = bufferData.length;
	}
	bufferData.index = 0;			// We do this so that queueFrame has a good starting point.
						// We also HAVE to change it because without this line, bufferData.index equals bufferMetadata.count + 1.
	queuedFramesCount = 0;
	initialized = true;
	return Error::none;

freeAndReturnError:
	// try to free the mmaps that didn't fail
	for (uint32_t i = 0; i < bufferData.index; i++) { munmap(frameLocations[i].start, frameLocations[i].size); }	// no need to handle error here

	delete[] frameLocations;

freeDeviceBuffersAndReturnError:
	bufferMetadata.count = 0;
	interruptedIoctl(fd, VIDIOC_REQBUFS, &bufferMetadata);
	return err;
}

Camera::Error Camera::init(uint32_t pixelFormat, uint32_t field) {
	Error err = readPreferredFormat(); if (err != Error::none) { return err; }
	format.fmt.pix.pixelformat = pixelFormat;
	format.fmt.pix.field = field;
	return init();
}

Camera::Error Camera::defaultInit() { return init(V4L2_PIX_FMT_RGB24, V4L2_FIELD_NONE); }

Camera::Error Camera::readStreamingParameters() {
	bzero(&streamingParameters, sizeof(streamingParameters));
	streamingParameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_G_PARM, &streamingParameters) == -1) { return Error::device_streaming_parameters_unavailable; }
	return Error::none;
}

// NOTE: We would have to worry about the capture standard if we were supporting old devices, modern, digital devices (webcams, etc...) don't have v4l2 standards.
// Capture standards set a minimum timePerFrame, which, following the above, we don't have to worry about here.
// Device itself may limit (upper and lower) the timePerFrame, setting invalid values to their nearest valid values.
Camera::Error Camera::setTimePerFrame(uint32_t numerator, uint32_t denominator) {
	Error err = readStreamingParameters(); if (err != Error::none) { return err; }

	if (streamingParameters.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
		streamingParameters.parm.capture.timeperframe.numerator = numerator;
		streamingParameters.parm.capture.timeperframe.denominator = denominator;
		if (interruptedIoctl(fd, VIDIOC_S_PARM, &streamingParameters) == -1) { return Error::device_set_streaming_parameters_failed; }
		return Error::none;
	}
	return Error::device_custom_timeperframe_unsupported;
}

Camera::Error Camera::getTimePerFrame(uint32_t& numerator, uint32_t& denominator) {
	Error err = readStreamingParameters(); if (err != Error::none) { return err; }
	numerator = streamingParameters.parm.capture.timeperframe.numerator;
	denominator = streamingParameters.parm.capture.timeperframe.denominator;
	return Error::none;
}

Camera::Error Camera::start() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_STREAMON, &type) == -1) { return Error::device_start_failed; }
	return Error::none;
}

Camera::Error Camera::queueFrame() {
	if (interruptedIoctl(fd, VIDIOC_QBUF, &bufferData) == -1) { return Error::device_queue_buffer_failed; }
	queuedFramesCount++;
	if (bufferData.index == lastBufferIndex) { bufferData.index = 0; return Error::none; }
	bufferData.index++;
	return Error::none;
}

// NOTE: If poll succeeds but sets POLLERR in pollStruct.revents, it could mean that there is nothing to dequeue.
// It could also mean some other stuff, the documentation isn't very good in my opinion. The important thing is:
// poll() doesn't always set POLLERR when there is nothing to dequeue, sometimes poll() will just hang.
// To avoid this ever happening, we have to build our own system for keeping track of queued frames.
// A counter will suffice.

Camera::Error Camera::dequeueFrame() {
	if (queuedFramesCount == 0) { return Error::dequeue_frame_impossible; }
	if (interruptedPoll(&pollStruct, 1, -1) == -1) { return Error::poll_failed; }
	if (pollStruct.revents & POLLERR) { return Error::dequeue_frame_impossible; }
	if (interruptedIoctl(fd, VIDIOC_DQBUF, &bufferData) == -1) { return Error::device_dequeue_buffer_failed; }
	queuedFramesCount--;
	return Error::none;
}

Camera::Error Camera::queueAllFrames() {
	bufferData.index = 0;
	Error err = queueFrame(); if (err != Error::none) { return err; }
	while (bufferData.index != 0) { err = queueFrame(); if (err != Error::none) { return err; } }
	return Error::none;
}

Camera::Error Camera::dequeueAllFrames() {
	while (queuedFramesCount != 0) { Error err = dequeueFrame(); if (err != Error::none) { return err; } }
	return Error::none;
}

Camera::Error Camera::shootFrame() {
	// NOTE: This doesn't throw anything if it encounters dequeue_frame_impossible because that just means that there are no more frames to dequeue, which is ok.
	Error err = dequeueAllFrames(); if (err != Error::none && err != Error::dequeue_frame_impossible) { return err; }
	err = queueFrame(); if (err != Error::none) { return err; }
	err = dequeueFrame(); if (err != Error::none) { return err; }
	return Error::none;
}

Camera::Error Camera::stop() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_STREAMOFF, &type) == -1) { return Error::device_stop_failed; }
	queuedFramesCount = 0;
	return Error::none;
}

Camera::Error Camera::free() {
	if (!initialized) { return Error::already_freed; }

	Error err = stop();
	if (err != Error::none) { return err; }

	for (uint32_t i = 0; i < bufferMetadata.count; i++) { if (munmap(frameLocations[i].start, frameLocations[i].size) == -1) { return Error::munmap_failed; } }
	delete[] frameLocations;

	bufferMetadata.count = 0;
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &bufferMetadata) == -1) { if (errno == EINVAL) { return Error::device_mmap_unsupported; } return Error::device_request_buffers_failed; }

	return Error::none;
}

Camera::Error Camera::close() {
	if (fd == -1) { return Error::already_closed; }

	// NOTE: The fact that free() frees the device buffers is useless here because closing fd does it anyway. I'm leaving it in free() to make calling free() directly
	// possible. Making close() a special case, where freeing the buffers is avoided because of redundancy, isn't worth it in my opinion.
	// Plus, it might not even improve performance or make the slightest amount of difference because the driver probably checks if the buffers are freed.
	// If they are, like we're doing right now, the driver probably skips unnecessary work.
	Error err = free();
	if (err != Error::none) { return err; }

	if (::close(fd) == -1) { fd = -1; return Error::file_close_failed; }
	fd = -1;

	return Error::none;
}

Camera::~Camera() { close(); }
