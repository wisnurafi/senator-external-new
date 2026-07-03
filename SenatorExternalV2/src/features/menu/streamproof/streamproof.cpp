#include "streamproof.h"

#include <settings.h>

namespace menu
{
	namespace streamproof
	{
		namespace
		{
			int g_last_state = -1;
		}

		void sync(HWND window)
		{
			if (window == nullptr)
				return;

			const int desired = settings::menu::streamproof ? 1 : 0;
			if (desired == g_last_state)
				return;

			SetWindowDisplayAffinity(window, desired ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
			g_last_state = desired;
		}

		void reset()
		{
			g_last_state = -1;
		}
	}
}
