#include <cstdint>

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
	deviceNoMMap = -10,
	deviceRequestBuffer = -11,
	deviceInsufficientMemory = -12,
	userInsufficientMemory = -13,
	deviceQueryBuf = -14,
	mMapFailed = -15,
	deviceMemFree = -16,
	deviceGetParm = -17,
	deviceSetParm = -18,
	deviceNoCustomFPS = -19,
	deviceStart = -20,
	deviceStop = -21,
	deviceShootQBuf = -22,
	deviceShootDQBuf = -23
};

class Camera {
public:
	struct BufferLocation { void* start; size_t size; };

	const char* deviceName;
	int fd;

	struct v4l2_format format;
	struct v4l2_requestbuffers buffers;

	BufferLocation* bufferLocations;

	void* data();		// Getter for frame data.
	
	Camera(const char* deviceName);

	Camera(Camera&& other);
	Camera& operator=(Camera&& other);

	Camera(const Camera& other) = delete;			// NOTE: This gets done implicitly since I've declared move functions, doing it anyway, design choice.
	Camera& operator=(const Camera& other) = delete;

	CameraError open();

	CameraError init();					// Needs to run after open().

	CameraError setFPS();					// Needs to run after open(), can run before init().
	uint32_t getFPS(CameraError& error);			// getFPS only yields round FPS values. If FPS is non-integer, rounds down.
	uint32_t getFPS();

	CameraError start();					// Needs to run after open() and after init(). TODO: See if setFPS can be run after start correctly.
	CameraError stop();

	CameraError free();

	CameraError close();					// Calls free() before closing, so don't call both.

	~Camera();
};
