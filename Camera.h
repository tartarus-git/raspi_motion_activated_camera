#include <cstdint>

#include <linux/videodev2.h>

enum class CameraError {
	none = 0,
	statusInfo = -1,
	fileNotDevice = -2,
	cannotOpen = -3,
	cannotClose = -4,
	deviceQueryCap = -5,
	deviceNoVideo = -6,
	deviceNoStreaming = -7,
	deviceGetFormat = -8,
	deviceNoMMap = -9,
	deviceRequestBuffer = -10,
	deviceInsufficientMemory = -11,
	userInsufficientMemory = -12,
	deviceQueryBuf = -13,
	mMapFailed = -14,
	deviceMemFree = -15,
	deviceGetParm = -16,
	deviceSetParm = -17,
	deviceNoCustomFPS = -18
};

class Camera {
public:
	struct BufferLocation { void* start; size_t size; };

	const char* deviceName;
	int fd;

	struct v4l2_format format;
	struct v4l2_requestbuffers buffers;

	BufferLocation* bufferLocations;
	
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

	CameraError free();

	CameraError close();					// Calls free() before closing, so don't call both.

	~Camera();
};
