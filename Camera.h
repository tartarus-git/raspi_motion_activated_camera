#include <cstdint>
#include <cstddef>
#include <poll.h>

#include <linux/videodev2.h>

enum class CameraError {		// TODO: Make these numbers make sense.
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

class Camera {
public:
	struct BufferLocation { void* start; size_t size; };

	const char* deviceName;
	int fd;

	struct pollfd pollStruct;

	struct v4l2_format format;
	struct v4l2_requestbuffers buffers;

	BufferLocation* bufferLocations;

	void* frameData();		// Getter for frame data.
	size_t frameSize();		// Getter for frame size.
	
	Camera(const char* deviceName);

	Camera& operator=(Camera&& other);
	Camera(Camera&& other);

	Camera(const Camera& other) = delete;			// NOTE: This gets done implicitly since I've declared move functions, doing it anyway, design choice.
	Camera& operator=(const Camera& other) = delete;

	CameraError open();

	CameraError init(uint32_t pixelFormat, uint32_t field);	// Needs to run after open().
	CameraError init();

	CameraError setFPS(uint32_t FPS);			// Needs to run after open(), can run before init().
	uint32_t getFPS(CameraError& error);			// getFPS only yields round FPS values. If FPS is non-integer, rounds down.
	uint32_t getFPS();

	CameraError start();					// Needs to run after open() and after init(). TODO: See if setFPS can be run after start correctly.

	CameraError shootFrame();

	CameraError stop();

	CameraError free();

	CameraError close();					// Calls free() before closing, so don't call both.

	~Camera();
};
