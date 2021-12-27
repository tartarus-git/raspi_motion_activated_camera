#include <cstddef>

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

		// opens the "/dev/fb0" device file
		Error open();

		// Fills variableInfo struct and uses the resulting data to calculate frameSize. Initializes the frame pointer to shared memory using mmap.
		Error init();

		// unmaps the frame pointer to shared memory
		Error free();

		// closes the device file
		Error close();
	};
}
