#include <cstdint>
#include <cstddef>
#include <poll.h>

#include <linux/videodev2.h>


class Camera {
public:
	enum class Error {
		none = 0,
		statusInfo = -1,
		fileNotDevice = -2,
		cannotOpen = -3,
		already_closed = -4,
		file_close_failed = -4,
		deviceQueryCap = -5,
		deviceNoVideo = -6,
		deviceNoStreaming = -7,
		deviceGetFormat = -8,
		deviceSetFormat = -9,
		deviceFormatUnsupported = -10,
		deviceNoMMap = -11,
		deviceRequestBuffer = -12,
		deviceInsufficientMemory = -13,
		userInsufficientMemory = -14,
		deviceQueryBuf = -15,
		mMapFailed = -16,
		mUnmapFailed = -17,
		deviceMemFree = -18,
		deviceGetParm = -19,
		deviceSetParm = -20,
		deviceNoCustomFPS = -21,
		deviceStart = -22,
		deviceStop = -23,
		deviceShootQBuf = -24,
		deviceShootDQBuf = -25,
		userShootPoll = -26,
		munmap_failed,
		device_mmap_unsupported,
		device_request_buffers_failed,
		device_free_buffers_failed,
		device_stop_failed,
		device_queue_buffer_failed,
		poll_failed,
		device_dequeue_buffer_failed,
		device_start_failed,
		device_set_streaming_parameters_failed,
		device_custom_timeperframe_unsupported,
		device_streaming_parameters_unavailable,
		device_format_unavailable,
		not_freed,
		device_video_capture_unsupported,
		device_streaming_unsupported,
		invalid_format_type,
		invalid_format_type,
		format_unsupported,
		device_buffer_request_failed,
		device_out_of_memory,
		mmap_failed,
		device_capabilities_unavailable,
		device_buffer_query_failed,
		user_out_of_memory,
		file_open_failed,
		file_is_not_device,
		status_info_unavailable
	};

	struct BufferLocation { void* start; size_t size; bool queued; };

	const char* deviceName;
	int fd;

	struct v4l2_capability capabilities;					// NOTE: Struct is included so as to know that v4l2_* is a struct. Design choice.
	struct v4l2_format format;
	
	struct v4l2_requestbuffers bufferMetadata;
	bool initialized = false;

	struct v4l2_buffer bufferData;

	struct v4l2_streamparm streamingParameters;

	struct pollfd pollStruct;

	// TODO: maybe add ifdefs around this getErrorMessage stuff in case the user doesn't want all of that const char* storage in his executable.
	const char* getErrorMessage(Error cameraError, int errno);

	BufferLocation* frameLocations;

	Camera(const char* deviceName);

	const char* getErrorMessage(Error cameraError, int errnoValue);

	Camera& operator=(Camera&& other);
	Camera(Camera&& other);

	Camera(const Camera& other) = delete;					// NOTE: This gets done implicitly since I've declared move functions, doing it anyway, design choice.
	Camera& operator=(const Camera& other) = delete;

	Error open();

	Error readCapabilities();				// Fills the capabilities structure. init() does this as well, no need to call both.
	bool supportsVideoCapture();
	bool supportsStreaming();

	Error readPreferredFormat();
	Error tryFormat();

	Error init();
	Error init(uint32_t pixelFormat, uint32_t field);	// Needs to run after open().
	Error defaultInit();

	Error readStreamingParameters();

	Error setTimePerFrame(uint32_t numerator, uint32_t denominator);			// Needs to run after open(), can run before init().
	Error getTimePerFrame(uint32_t& numerator, uint32_t& denominator);			// getFPS only yields round FPS values. If FPS is non-integer, rounds down.

	Error start();					// Needs to run after open() and after init(). TODO: See if setFPS can be run after start correctly.

	Error queueAllFrames();
	Error dequeueAllFrames();

	Error dequeueAllQueuedFrames();

	Error shootFrame();

	Error stop();

	Error free();

	Error close();					// Calls free() before closing, so don't call both.

	~Camera();
};
