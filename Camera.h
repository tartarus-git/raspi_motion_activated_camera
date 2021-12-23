#include <cstdint>
#include <cstddef>
#include <poll.h>

#include <linux/videodev2.h>

class Camera {
public:
	enum class Error {			// TODO: These errors need to have the correct numbers, already sorted though.
		none = 0,
		status_info_unavailable = -1,
		file_is_not_device = -2,
		file_open_failed = -3,
		device_capabilities_unavailable = -4,
		device_format_unavailable = -5,
		not_freed = -6,
		device_video_capture_unsupported = -7,
		device_streaming_unsupported = -8,
		invalid_format_type = -9,
		format_unsupported = -10,
		device_buffer_request_failed = -11,
		device_out_of_memory = -12,
		user_out_of_memory = -13,
		device_buffer_query_failed = -14,
		mmap_failed = -15,
		device_streaming_parameters_unavailable = -16,
		device_set_streaming_parameters_failed = -17,
		device_custom_timeperframe_unsupported = -18,
		device_start_failed = -19,
		device_queue_buffer_failed = -20,
		poll_failed = -21,
		stream_not_started = 1,
		device_dequeue_buffer_failed = -22,
		device_stop_failed = -23,
		already_freed = -24,
		munmap_failed = -25,
		device_mmap_unsupported = -26,
		device_request_buffers_failed = -27,
		already_closed = -28,
		file_close_failed = -29
	};

	const char* deviceName;
	int fd;

	struct pollfd pollStruct;

	struct v4l2_capability capabilities;					// NOTE: Struct is included so as to know that v4l2_* is a struct. Design choice.
	struct v4l2_format format;
	
	struct v4l2_requestbuffers bufferMetadata;
	uint32_t lastBufferIndex;
	bool initialized = false;

	struct v4l2_buffer bufferData;

	struct BufferLocation { void* start; size_t size; bool queued; }* frameLocations;

	struct v4l2_streamparm streamingParameters;

	Camera(const char* deviceName);

	Camera& operator=(Camera&& other);
	Camera(Camera&& other);

	Camera(const Camera& other) = delete;					// NOTE: This gets done implicitly since I've declared move functions, doing it anyway, design choice.
	Camera& operator=(const Camera& other) = delete;

	// open device file and set pollStruct file descriptor
	Error open();

	// Reads device capabilities and fills capabilities struct. init() does this as well, no need to call both.
	Error readCapabilities();
	
	bool supportsVideoCapture();			// returns true if device supports video capture, otherwise returns false
	bool supportsStreaming();			// returns true if device supports streaming (queueing and dequeueing buffers in shared memory), otherwise returns false

	// Reads the device's preferred format and fills the format struct. init(uint32_t, uint32_t) and defaultInit() do this as well, no need to call both.
	Error readPreferredFormat();
	// Asks the device if the format in the format struct is acceptable. If it isn't, device changes format struct to nearest valid configuration. If it is, format struct stays the way it is.
	Error tryFormat();

	// initialization must be performed after opening device

	// Try to use format data in format struct to initialize device format. Also initialize device buffers and initialize shared memory access using mmap.
	Error init();
	// TODO: Add function descriptions to the rest of all these, order them in the header and in the implementation files, and go through the whole thig with principles in mind, then make main.cpp acceptable and write a small test program. Save the test capture as ppm because it's easy.
	// Same function as init(), except that it reads the device's preferred format, changes the pixelformat and field options to the specified values, and then initializes the device with the resulting format.
	Error init(uint32_t pixelFormat, uint32_t field);	// Needs to run after open().
	// Same as init(uint32_t, uint32_t), except that it uses V4L2_PIX_FMT_RGB24 as pixelFormat and V4L2_FIELD_NONE as field.
	Error defaultInit();

	// reads streaming parameters from device and fills streamingParameters struct
	Error readStreamingParameters();

	// Time per frame functions need to be called after opening device, but can be called before initializing device.
	// Depending on the device, you may be able to call time per frame functions after starting stream as well.

	// Try to set device's time per frame. Device might round to nearest acceptable value if one passes in a value that is deemed unacceptable.
	Error setTimePerFrame(uint32_t numerator, uint32_t denominator);			// Needs to run after open(), can run before init().
	Error getTimePerFrame(uint32_t& numerator, uint32_t& denominator);			// getFPS only yields round FPS values. If FPS is non-integer, rounds down.

	// Start streaming. Needs to be called after opening and initializing the device.
	Error start();

	// Frames can be queued before calling start(), they just won't get filled before calling start().
	// dequeueFrame() and dequeueAllFrames() will infinitely hang if called before start(). dequeueAllQueuedFrames() won't unless frames were queued before start().

	// Queue the frame at bufferData.index. Increments bufferData.index.
	Error queueFrame();

	// Dequeue the frame that was finished the earliest. Sets bufferData.index to the index of the newly dequeued frame.
	Error dequeueFrame();

	// Queue all frames. bufferData.index equals 0 after function returns.
	Error queueAllFrames();

	// Dequeue all frames. bufferData.index is set to the index of the last frame that was dequeued. Infinitely hangs if one of the frames wasn't queued.
	Error dequeueAllFrames();

	// Dequeues only frames that were queued and thereby doesn't hang because of unqueued frames. This advantage over dequeueAllFrames() comes at the cost of more computation.
	Error dequeueAllQueuedFrames();

	// Shoot a single (new) frame. The resulting frame is as recent as possible. Calls dequeueAllQueuedFrames(), then queueFrame() and then dequeueFrame(), so it's not cheap.
	Error shootFrame();

	// Stop streaming. All frames that haven't been dequeued yet are lost. Needs to be called after start().
	Error stop();

	// Free device and user resources. This function is the counterpart to init(). Stops the stream if that hasn't been done already.
	Error free();

	// Close device. This function is the counterpart to open(). Calls free().
	Error close();

	~Camera();			// calls close()
};
