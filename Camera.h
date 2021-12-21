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
		cannotClose = -4,
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
		userShootPoll = -26
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


	BufferLocation* frameLocations;

	Camera(const char* deviceName);

	Camera& operator=(Camera&& other);
	Camera(Camera&& other);

	Camera(const Camera& other) = delete;					// NOTE: This gets done implicitly since I've declared move functions, doing it anyway, design choice.
	Camera& operator=(const Camera& other) = delete;

	CameraError open();

	CameraError readCapabilities();				// Fills the capabilities structure. init() does this as well, no need to call both.
	bool supportsVideoCapture();
	bool supportsStreaming();

	CameraError readPreferredFormat();
	CameraError tryFormat();

	CameraError init(uint32_t pixelFormat, uint32_t field);	// Needs to run after open().
	CameraError init();

	Error readStreamingParameters();

	Error setTimePerFrame(uint32_t FPS);			// Needs to run after open(), can run before init().
	Error getTimePerFrame(uint32_t numerator, uint32_t denominator);			// getFPS only yields round FPS values. If FPS is non-integer, rounds down.

	CameraError start();					// Needs to run after open() and after init(). TODO: See if setFPS can be run after start correctly.

	Error queueAllFrames();
	Error dequeueAllFrames();

	Error dequeueAllQueuedFrames();

	CameraError shootFrame();

	CameraError stop();

	CameraError free();

	CameraError close();					// Calls free() before closing, so don't call both.

	~Camera();
};
