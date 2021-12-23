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
#include <cstring>

#include <linux/videodev2.h>

// TODO: See if you can change contents of const char* by casting to char*
const char* Camera::getErrorMessage(Error cameraError, int errnoValue) {
	switch (cameraError) {
	case Error::none: return nullptr;
	case Error::status_info_unavailable: return "failed to get device file status info";
	case Error::file_is_not_device: return "the specified file does not represent a device";
					// TODO: etc...
	}
}

int interruptedIoctl(int fd, unsigned long request, void* argp) {
	int returnValue;
	do { returnValue = ioctl(fd, request, argp); }
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

Camera& Camera::operator=(Camera&& other) {			// TODO: Make sure all necessary fields are getting copied here and you didn't forget anything.
	deviceName = other.deviceName;
	fd = other.fd;
	capabilities = other.capabilities;
	format = other.format;
	bufferMetadata = other.bufferMetadata;
	bufferData = other.bufferData;
	streamingParameters = other.streamingParameters;

	frameLocations = other.frameLocations;

	initialized = false;
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

Camera::Error Camera::tryFormat() {
	if (interruptedIoctl(fd, VIDIOC_TRY_FMT, &format) == -1) { return Error::device_format_unavailable; }
	return Error::none;
}

Camera::Error Camera::init() {
	if (initialized) { return Error::not_freed; }			// don't allow initialization before freeing previous initialization

	// See if the camera supports capture and streaming. Necessary for capturing images through mmap.

	Error err = readCapabilities();
	if (err != Error::none) { return err; }

	if (!supportsVideoCapture()) { return Error::device_video_capture_unsupported; }
	if (!supportsStreaming()) { return Error::device_streaming_unsupported; }

	// negotiate device format

	v4l2_format previousFormatState = format;
	if (interruptedIoctl(fd, VIDIOC_S_FMT, &format) == -1) { return Error::invalid_format_type; }
	if (memcmp(&format, &previousFormatState, sizeof(v4l2_format)) != 0) { return Error::format_unsupported; }
	// If the check didn't fail, the format was set on the device, so we can continue.

	// initialize device buffers and set up shared memory access

	if (bufferMetadata.count == 0) { bufferMetadata.count = 1; }
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &bufferMetadata) == -1) { return Error::device_buffer_request_failed; }
	if (bufferMetadata.count == 0) { return Error::device_out_of_memory; }

	frameLocations = (BufferLocation*)calloc(bufferMetadata.count, sizeof(BufferLocation));
	if (!frameLocations) { err = Error::user_out_of_memory; goto freeDeviceBuffersAndReturnError; }

	for (bufferData.index = 0; bufferData.index < bufferMetadata.count; bufferData.index++) {
		if (interruptedIoctl(fd, VIDIOC_QUERYBUF, &bufferData) == -1) { err = Error::device_buffer_query_failed; goto freeAndReturnError; }

		frameLocations[bufferData.index].start = mmap(nullptr, bufferData.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bufferData.m.offset);
		if (frameLocations[bufferData.index].start == MAP_FAILED) { err = Error::mmap_failed; goto freeAndReturnError; }
		frameLocations[bufferData.index].size = bufferData.length;
		frameLocations[bufferData.index].queued = false;
	}

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
	Error err = readPreferredFormat();
	if (err != Error::none) { return err; }
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
// NOTE: Capture standards set a minimum timePerFrame, which, following the above, we don't have to worry about here.
// NOTE: Device itself may limit (upper and lower) the timePerFrame, setting invalid values to their nearest valid values.
Camera::Error Camera::setTimePerFrame(uint32_t numerator, uint32_t denominator) {
	Error err = readStreamingParameters();
	if (err != Error::none) { return err; }
	if (streamingParameters.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
		streamingParameters.parm.capture.timeperframe.numerator = numerator;
		streamingParameters.parm.capture.timeperframe.denominator = denominator;
		if (interruptedIoctl(fd, VIDIOC_S_PARM, &streamingParameters) == -1) { return Error::device_set_streaming_parameters_failed; }
		return Error::none;
	}
	return Error::device_custom_timeperframe_unsupported;
}

Camera::Error Camera::getTimePerFrame(uint32_t& numerator, uint32_t& denominator) {
	Error err = readStreamingParameters();
	if (err != Error::none) { return err; }
	numerator = streamingParameters.parm.capture.timeperframe.numerator;
	denominator = streamingParameters.parm.capture.timeperframe.denominator;
	return Error::none;
}

Camera::Error Camera::start() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_STREAMON, &type) == -1) { return Error::device_start_failed; }
	return Error::none;
}

// NOTE: Put this somewhere nice-looking: You can queue all you want to, even before starting the stream, they just wont get processed before starting the stream.
Camera::Error Camera::queueAllFrames() {
	for (bufferData.index = 0; bufferData.index < bufferMetadata.count; bufferData.index++) {
		if (interruptedIoctl(fd, VIDIOC_QBUF, &bufferData) == -1) { return Error::device_queue_buffer_failed; }
		frameLocations[bufferData.index].queued = true;
	}
	return Error::none;
}

Camera::Error Camera::dequeueAllFrames() {
	for (unsigned int i = 0; i < bufferMetadata.count; i++) {
		if (poll(&pollStruct, 1, -1) == -1) { return Error::poll_failed; }
		if (pollStruct.revents & POLLERR) { return Error::poll_failed; }		// TODO: Make sure this is necessary.
		if (interruptedIoctl(fd, VIDIOC_DQBUF, &bufferData) == -1) { return Error::device_dequeue_buffer_failed; }
		frameLocations[bufferData.index].queued = false;
	}
	return Error::none;
}

Camera::Error Camera::dequeueAllQueuedFrames() {
	unsigned int extraQueuedBuffers = 0;
	for (unsigned int i = 0; i < bufferMetadata.count; i++) {
		if (frameLocations[i].queued) {
			if (poll(&pollStruct, 1, -1) == -1) { return Error::poll_failed; }
			if (pollStruct.revents & POLLERR) { return Error::poll_failed; }		// TODO: Make sure this is necessary.
			if (interruptedIoctl(fd, VIDIOC_DQBUF, &bufferData) == -1) { return Error::device_dequeue_buffer_failed; }
			if (bufferData.index == i) { frameLocations[i].queued = false; continue; }
			frameLocations[bufferData.index].queued = false;
			extraQueuedBuffers++;
		}
	}
	for (unsigned int i = 0; i < extraQueuedBuffers; i++) {
		if (poll(&pollStruct, 1, -1) == -1) { return Error::poll_failed; }
		if (pollStruct.revents & POLLERR) { return Error::poll_failed; }		// TODO: Make sure this is necessary.
		if (interruptedIoctl(fd, VIDIOC_DQBUF, &bufferData) == -1) { return Error::device_dequeue_buffer_failed; }
		frameLocations[bufferData.index].queued = false;
	}
	return Error::none;
}

Camera::Error Camera::shootFrame() {
	Error err = dequeueAllQueuedFrames();
	if (err != Error::none) { return err; }
	bufferData.index = 0;
	if (interruptedIoctl(fd, VIDIOC_QBUF, &bufferData) == -1) { return Error::device_queue_buffer_failed; }
	frameLocations[0].queued = true;
	if (poll(&pollStruct, 1, -1) == -1) { return Error::poll_failed; }
	if (pollStruct.revents & POLLERR) { return Error::poll_failed; }
	if (interruptedIoctl(fd, VIDIOC_DQBUF, &bufferData) == -1) { return Error::device_dequeue_buffer_failed; }
	frameLocations[0].queued = false;
	return Error::none;
}

Camera::Error Camera::stop() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (interruptedIoctl(fd, VIDIOC_STREAMOFF, &type) == -1) { return Error::device_stop_failed; }
	return Error::none;
}

Camera::Error Camera::free() {
	if (bufferMetadata.count == 0) { return Error::none; }

	Error err = stop();
	if (err != Error::none) { return err; }

	for (uint32_t i = 0; i < bufferMetadata.count; i++) { if (munmap(frameLocations[i].start, frameLocations[i].size) == -1) { return Error::munmap_failed; } }
	delete[] frameLocations;
	// Temp code:
	unsigned int tempCount = bufferMetadata.count;
	bufferMetadata.count = 0;
	if (interruptedIoctl(fd, VIDIOC_REQBUFS, &bufferData) == -1) { bufferMetadata.count = tempCount; if (errno == EINVAL) { return Error::device_mmap_unsupported; } return Error::device_request_buffers_failed; }
	if (bufferMetadata.count != 0) { return Error::device_free_buffers_failed; }
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
