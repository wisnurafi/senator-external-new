#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace netutil
{
	// HTTPS GET via WinHTTP. Returns true on success and fills `out` with body bytes.
	// Caller decides retry / timeout policy. Pass full URL ("https://host/path").
	inline bool https_get(const std::string& url, std::string& out)
	{
		out.clear();

		std::wstring wurl(url.begin(), url.end());
		URL_COMPONENTS uc{};
		uc.dwStructSize = sizeof(uc);
		wchar_t host[256]{};
		wchar_t path[2048]{};
		wchar_t extra[2048]{};
		uc.lpszHostName = host;
		uc.dwHostNameLength = _countof(host);
		uc.lpszUrlPath = path;
		uc.dwUrlPathLength = _countof(path);
		uc.lpszExtraInfo = extra;
		uc.dwExtraInfoLength = _countof(extra);

		if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
			return false;

		HINTERNET session = WinHttpOpen(L"SenatorExternalV2/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session) return false;

		HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
		if (!connect) { WinHttpCloseHandle(session); return false; }

		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		std::wstring full_path = path;
		full_path += extra;
		HINTERNET request = WinHttpOpenRequest(connect, L"GET", full_path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false; }

		bool ok = false;
		if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			WinHttpReceiveResponse(request, nullptr))
		{
			DWORD status = 0, sz = sizeof(status);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
			if (status == 200)
			{
				DWORD available = 0;
				do
				{
					available = 0;
					if (!WinHttpQueryDataAvailable(request, &available)) break;
					if (available == 0) break;
					std::vector<char> chunk(available);
					DWORD read = 0;
					if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
					out.append(chunk.data(), read);
				} while (available > 0);
				ok = !out.empty();
			}
		}

		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return ok;
	}
}
