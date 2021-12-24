#include <cstdint>
#include <cstddef>
#include <poll.h>

#include <linux/videodev2.h>

class Camera {
public:
	// Simulation of a scoped enum which, contrary to normal scoped enums, can be implicitly converted to integral types.
	struct Error {
		enum ErrorValue {
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
			dequeue_frame_impossible = -22,
			device_dequeue_buffer_failed = -23,
			device_stop_failed = -24,
			already_freed = -25,
			munmap_failed = -26,
			device_mmap_unsupported = -27,
			device_request_buffers_failed = -28,
			already_closed = -29,
			file_close_failed = -30
		};

	private: ErrorValue value;
	public:
		 Error(ErrorValue value) noexcept;	// Takes care of assignment operator too because ErrorValue is converted to Error first and then assigned.
		 operator int() const noexcept;		// Takes care of other integral types as well because resulting int can be implicitly converted to those.

		 // You'll still be able to do stuff like Camera::Error::ErrorValue x = Camera::Error::ErrorValue::status_info_unavailable.
		 // AFAIK, there is no way to avoid that, which sucks because that is not how the Camera::Error scoped enum would behave, but it is what it is.
	};

	const char* deviceName;
	int fd;

	struct pollfd pollStruct;

	struct v4l2_capability capabilities;					// NOTE: Struct is included so as to know that v4l2_* is a struct. Design choice.
	struct v4l2_format format;
	
	struct v4l2_requestbuffers bufferMetadata;
	uint32_t lastBufferIndex;
	uint32_t queuedFramesCount;
	bool initialized = false;

	struct v4l2_buffer bufferData;

	struct BufferLocation { void* start; size_t size; }* frameLocations;

	struct v4l2_streamparm streamingParameters;

	explicit Camera(const char* deviceName);				// NOTE: Explicit keyword prevents this from being used as a converting constructor.
										// Without this, one could pass "/dev/video0" into a Camera parameter, which doesn't look good in my opinion.
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
	// If bufferMetadata.count is 0 while calling this function, init() tries to allocate a single buffer. If bufferMetadata.count isn't 0, init() tries to allocate
	// bufferMetadata.count buffers. The amount of actually allocated buffers (which can be lower or 0 if the device runs out of memory, or higher if the device 
	// requires a certain amount of buffers to function properly) is stored in bufferMetadata.count after the function returns.
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

	// Start streaming. Needs to be called after opening and initializing the device. Can be called n-times before stop().
	// If that is the case, the function has no effects and doesn't return an error.
	Error start();

	// Frames can be queued before calling start(), they just won't get filled before calling start().

	// Queue the frame at bufferData.index. Increments bufferData.index.
	Error queueFrame();

	// Dequeue the frame that was finished the earliest. Sets bufferData.index to the index of the newly dequeued frame.
	// If called before start() or called when no frames are queued, returns Error::dequeue_frame_impossible.
	Error dequeueFrame();

	// Queue all frames. bufferData.index equals 0 after function returns.
	Error queueAllFrames();

	// Dequeue all queued frames. bufferData.index is set to the index of the most recently dequeued frame.
	// If called before start(), returns Error::dequeue_frame_impossible.
	Error dequeueAllFrames();

	// Shoot a single (new) frame. The resulting frame is as recent as possible. Calls dequeueAllFrames(), then queueFrame() and then dequeueFrame().
	// Sets bufferData.index to the index of the frame that was used for the frame.
	Error shootFrame();

	// Stop streaming. This function is the counterpart to start(). All frames that haven't been dequeued yet are lost.
	// stop() can be called before calling start(), it doesn't do anything and doesn't return an error. It does however cause all queued frames to be lost.
	Error stop();

	// Free device and user resources. This function is the counterpart to init(). Stops the stream if that hasn't been done already.
	Error free();

	// Close device. This function is the counterpart to open(). Calls free().
	Error close();

	~Camera();			// calls close()
};
