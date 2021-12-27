#include <cstddef>
#include <cstdint>

#include <linux/fb.h>

namespace vid {
	class Screen {
	public:
		struct Error {
			enum ErrorValue {
				none = 0,
				not_closed = -1,
				file_open_failed = -2,
				not_freed = -3,
				device_variable_info_unavailable = -4,
				mmap_failed = -5,
				already_freed = -6,
				munmap_failed = -7,
				already_closed = -8,
				file_close_failed = -9
			};

		private: ErrorValue value;
		public:
			 Error(ErrorValue value) noexcept;
			 operator int() const noexcept;
		};

		int fd = -1;

		bool initialized = false;

		fb_var_screeninfo variableInfo;

		// There aren't any getters for these because I think calling those is kind of annoying. Even though you can change these variables because of that, you probably shouldn't.
		void* frame;
		size_t frameSize;

		// NOTE: I've commented out some of these because I'm not sure how relevant they are
		// NOTE: for digital HDMI output. Frame buffer docs are horrible, but (based on tests)
		// NOTE: they don't seem to do anything.
		// Gets the virtual width of the screen. This may be clipped by monitor edges.
		//uint32_t virtualWidth() const noexcept;
		// Gets the virtual height of the screen. This may be clipped by monitor edges.
		//uint32_t virtualHeight() const noexcept;
		// gets the visible width of the screen
		uint32_t width() const noexcept;
		// gets the visible height of the screen
		uint32_t height() const noexcept;
		// gets offset of left edge of visible screen from left edge of virtual screen
		//uint32_t xOffset() const noexcept;
		// gets offset of top edge of visible screen from top edge of virtual screen
		//uint32_t yOffset() const noexcept;

		// opens the "/dev/fb0" device file
		Error open();

		// Fills variableInfo struct and uses the resulting data to calculate frameSize. Initializes the frame pointer to shared memory using mmap.
		Error init();

		// unmaps the frame pointer to shared memory
		Error free();

		// closes the device file
		Error close();

		~Screen();
	};
}
