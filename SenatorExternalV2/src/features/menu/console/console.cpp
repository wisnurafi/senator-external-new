#include "console.h"
#include "../../../settings.h"
#include <features/config/config.h>
#include <runtime/runtime.h>
#include <windows.h>
#include <thread>
#include <chrono>

namespace menu
{
	namespace console
	{
		void run()
		{
			using namespace std::chrono_literals;

			static bool was_hidden = false;

			for (; runtime::alive(); )
			{
				std::this_thread::sleep_for(50ms);

				bool should_hide = external_config::hide_console;

				HWND console_window = GetConsoleWindow();
				if (!console_window)
				{
					continue;
				}

				if (should_hide)
				{
					if (!was_hidden || IsWindowVisible(console_window))
					{
						ShowWindow(console_window, SW_HIDE);
						was_hidden = true;
					}
				}
				else
				{
					if (was_hidden || !IsWindowVisible(console_window))
					{
						ShowWindow(console_window, SW_SHOW);
						SetForegroundWindow(console_window);
						was_hidden = false;
					}
				}
			}
		}
	}
}
