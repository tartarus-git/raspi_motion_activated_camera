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
			not_closed = -1,
			status_info_unavailable = -2,
			file_is_not_device = -3,
			file_open_failed = -4,
			device_capabilities_unavailable = -5,
			device_cropping_unsupported = -6,
			device_cropping_capabilities_unavailable = -7,
			device_crop_unavailable = -8,
			device_format_unavailable = -9,
			not_freed = -10,
			device_video_capture_unsupported = -11,
			device_streaming_unsupported = -12,
			device_set_format_failed = -13,
			format_unsupported = -14,
			device_buffer_request_failed = -15,
			device_out_of_memory = -16,
			user_out_of_memory = -17,
			device_buffer_query_failed = -18,
			mmap_failed = -19,
			device_streaming_parameters_unavailable = -20,
			device_start_failed = -21,
			device_frame_data_unavailable = -22,
			device_queue_buffer_failed = -23,
			dequeue_frame_impossible = -24,
			poll_failed = -25,
			device_dequeue_buffer_failed = -26,
			device_stop_failed = -27,
			already_freed = -28,
			munmap_failed = -29,
			device_mmap_unsupported = -30,
			already_closed = -31,
			file_close_failed = -32
		};

	private: ErrorValue value;
	public:
		 Error(ErrorValue value) noexcept;	// Takes care of assignment operator too because ErrorValue is converted to Error first and then assigned.
		 operator int() const noexcept;		// Takes care of other integral types as well because resulting int can be implicitly converted to those.

		 // You'll still be able to do stuff like Camera::Error::ErrorValue x = Camera::Error::ErrorValue::status_info_unavailable.
		 // AFAIK, there is no way to avoid that, which sucks because that is not how the Camera::Error scoped enum would behave, but it is what it is.
	};

	const char* deviceName;
	int fd = -1;

	struct pollfd pollStruct;						// NOTE: struct keyword not necessary, putting it in because design choice

	struct v4l2_capability capabilities;

	struct v4l2_cropcap croppingCapabilities;
	struct v4l2_crop crop;

	struct v4l2_format format;
	
	struct v4l2_requestbuffers bufferMetadata;
	uint32_t lastBufferIndex;
	uint32_t queuedFramesCount;
	bool initialized = false;

	struct v4l2_buffer bufferData;

	struct BufferLocation { void* start; size_t size; }* frameLocations;

	struct v4l2_streamparm streamingParameters;

	explicit Camera(const char* deviceName) noexcept;				// NOTE: Explicit keyword prevents this from being used as a converting constructor.
											// Without this, one could pass "/dev/video0" into a Camera parameter, which doesn't look good in my opinion.
	Camera& operator=(Camera&& other) noexcept;
	Camera(Camera&& other) noexcept;

	Camera(const Camera& other) = delete;						// NOTE: This gets done implicitly since I've declared move functions, doing it anyway, design choice.
	Camera& operator=(const Camera& other) = delete;

	// open device file and set pollStruct file descriptor
	Error open();

	// Reads device capabilities and fills capabilities struct. init() does this as well, no need to call both.
	Error readCapabilities();
	
	bool supportsVideoCapture() const noexcept;					// returns true if device supports video capture, otherwise returns false
	bool supportsStreaming() const noexcept;					// returns true if device supports streaming (queueing and dequeueing buffers in shared memory), otherwise returns false

	// Changing cropping parameters can change what format (mainly width and height I think) the device deems acceptable. This is because of aspect ratio.
	// Bottom line is: When negotiating cropping parameters and format scaling parameters, you should beware of the fact that one affects the other.

	// reads device cropping capabilities and fills croppingCapabilities struct
	Error readCroppingCapabilities();

	// reads device crop state and fills crop struct
	Error readCrop();

	// writes crop state in crop struct to device
	Error writeCrop();

	// changes the bounds of the crop struct and writes the resulting crop struct to device
	Error writeCrop(int32_t left, int32_t top, int32_t width, int32_t bottom);

	// Checks whether cropping is supported and, if so, sets crop to the default crop for the device and writes updated version to device.
	// Returns an error only if something goes wrong. If the problem is only that cropping is unsupported, the function just doesn't do anything.
	Error writeDefaultCropIfSupported();

	// Reads the device's current format and fills the format struct. init(uint32_t, uint32_t) and defaultInit() do this as well, no need to call both.
	Error readFormat();
	// Asks the device if the format in the format struct is acceptable. If it isn't, device changes format struct to nearest valid configuration. If it is, format struct stays the way it is.
	Error tryFormat();

	// initialization must be performed after opening device

	// Try to use format data in format struct to initialize device format. Also initialize device buffers and initialize shared memory access using mmap.
	// If bufferMetadata.count is 0 while calling this function, init() tries to allocate a single buffer. If bufferMetadata.count isn't 0, init() tries to allocate
	// bufferMetadata.count buffers. The amount of actually allocated buffers (which can be lower or 0 if the device runs out of memory, or higher if the device 
	// requires a certain amount of buffers to function properly) is stored in bufferMetadata.count after the function returns.
	Error init();
	// Same function as init(), except that it reads the device's current format, changes the pixelformat and field options to the specified values, and then initializes the device with the resulting format.
	Error init(uint32_t pixelFormat, uint32_t field);	// Needs to run after open().
	// Same as init(uint32_t, uint32_t), except that it uses V4L2_PIX_FMT_RGB24 as pixelFormat and V4L2_FIELD_NONE as field.
	Error defaultInit();

	// Streaming parameter functions need to be called after opening device, but can be called before or after initializing device.
	// Depending on the device, you may be able to call time per frame functions after starting stream as well.

	// reads streaming parameters from device and fills streamingParameters struct
	Error readStreamingParameters();

	// returns true if one can change the time per frame of the device, otherwise returns false
	bool supportsCustomTimePerFrame() const noexcept;

	// sets time per frame data in the streamingParameters struct
	void setTimePerFrame(uint32_t numerator, uint32_t denominator) noexcept;

	// gets time per frame data from the streamingParameters struct
	void getTimePerFrame(uint32_t& numerator, uint32_t& denominator) const noexcept;

	// Writes streaming parameters from streamingParameters struct into device. Device may change these to nearest valid values if it deems them invalid.
	Error writeStreamingParameters();

	// Start streaming. Needs to be called after opening and initializing the device. Can be called n-times before stop().
	// If that is the case, the function has no effects and doesn't return an error.
	Error start() const;

	// Frames can be queued before calling start(), they just won't get filled before calling start().
	// readFrameData() can also be called before start().

	// Queries the device for the frame data of the frame at bufferData.index. Fills bufferData with the data.
	// This gets done in every queue/dequeue function, so you don't need to call this all the time.
	Error readFrameData();

	// Returns true if V4L2_BUF_FLAG_ERROR is set in the current bufferData. This means that you can continue operation as normal, but the current frame may be corrupted.
	bool isFrameCorrupted() const noexcept;

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
