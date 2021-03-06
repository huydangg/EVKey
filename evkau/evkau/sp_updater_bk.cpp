#include "prehdr.h"
#include "sp_updater.h"
#include <vector>
#include <iterator>  
#include <list>  
#include "curl.h"
#include "c_download_dlg.h"
#include <string>
#include <experimental/filesystem>
//#include <Winhttp.h>
//#include <WinInet.h>

namespace fs = std::experimental::filesystem;

const wchar_t* g_server = L"raw.githubusercontent.com";
const char* g_link = "https://raw.githubusercontent.com/lamquangminh/EVKey/master/release/";


typedef LPVOID HINTERNET;
typedef HINTERNET * LPHINTERNET;
typedef WORD INTERNET_PORT;
typedef INTERNET_PORT * LPINTERNET_PORT;
#if (defined(_M_IX86) || defined(_M_IA64) || defined(_M_AMD64)) && !defined(MIDL_PASS)
#define DECLSPEC_IMPORT __declspec(dllimport)
#else
#define DECLSPEC_IMPORT
#endif
#if !defined(_WINHTTP_INTERNAL_)
#define WINHTTPAPI DECLSPEC_IMPORT
#else
#define WINHTTPAPI
#endif
#define INTERNET_DEFAULT_PORT           0           // use the protocol-specific default
#define INTERNET_DEFAULT_HTTP_PORT      80          //    "     "  HTTP   "
#define INTERNET_DEFAULT_HTTPS_PORT     443         //    "     "  HTTPS  "
#define WINHTTP_FLAG_ASYNC              0x10000000  // this session is asynchronous (where supported)
#define WINHTTP_FLAG_SECURE                0x00800000  // use SSL if applicable (HTTPS)
#define WINHTTP_FLAG_ESCAPE_PERCENT        0x00000004  // if escaping enabled, escape percent as well
#define WINHTTP_FLAG_NULL_CODEPAGE         0x00000008  // assume all symbols are ASCII, use fast convertion
#define WINHTTP_FLAG_BYPASS_PROXY_CACHE    0x00000100 // add "pragma: no-cache" request header
#define	WINHTTP_FLAG_REFRESH               WINHTTP_FLAG_BYPASS_PROXY_CACHE
#define WINHTTP_FLAG_ESCAPE_DISABLE        0x00000040  // disable escaping
#define WINHTTP_FLAG_ESCAPE_DISABLE_QUERY  0x00000080  // if escaping enabled escape path part, but do not escape query
#define SECURITY_FLAG_IGNORE_UNKNOWN_CA         0x00000100
#define SECURITY_FLAG_IGNORE_CERT_DATE_INVALID  0x00002000 // expired X509 Cert.
#define SECURITY_FLAG_IGNORE_CERT_CN_INVALID    0x00001000 // bad common name in X509 Cert.
#define SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE   0x00000200
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY               0
#define WINHTTP_ACCESS_TYPE_NO_PROXY                    1
#define WINHTTP_ACCESS_TYPE_NAMED_PROXY                 3
#define WINHTTP_NO_PROXY_NAME     NULL
#define WINHTTP_NO_PROXY_BYPASS   NULL
#define WINHTTP_NO_REFERER             NULL
#define WINHTTP_DEFAULT_ACCEPT_TYPES   NULL
#define WINHTTP_NO_ADDITIONAL_HEADERS   NULL
#define WINHTTP_NO_REQUEST_DATA         NULL

typedef HINTERNET(WINAPI *lpWinHttpOpen)
(LPCWSTR pszAgentW, DWORD dwAccessType, LPCWSTR pszProxyW, LPCWSTR pszProxyBypassW, DWORD dwFlags);
lpWinHttpOpen http_open;

typedef HINTERNET(WINAPI *lpWinHttpConnect)
(HINTERNET hSession, LPCWSTR pswzServerName, INTERNET_PORT nServerPort, DWORD dwReserved);
lpWinHttpConnect http_connect;

typedef HINTERNET(WINAPI *lpWinHttpOpenRequest)
(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName, LPCWSTR pwszVersion, LPCWSTR pwszReferrer OPTIONAL, LPCWSTR FAR * ppwszAcceptTypes OPTIONAL, DWORD dwFlags);
lpWinHttpOpenRequest http_open_request;

typedef BOOL (WINAPI *lpWinHttpSendRequest)
(HINTERNET hRequest, LPCWSTR pwszHeaders OPTIONAL, DWORD dwHeadersLength, LPVOID lpOptional OPTIONAL, DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext);
lpWinHttpSendRequest http_send_request;

typedef BOOL(WINAPI *lpWinHttpReceiveResponse)
(HINTERNET hRequest, LPVOID lpReserved);
lpWinHttpReceiveResponse http_receive_response;

typedef BOOL (WINAPI *lpWinHttpQueryDataAvailable)
(HINTERNET hRequest, LPDWORD lpdwNumberOfBytesAvailable);
lpWinHttpQueryDataAvailable http_query_data;

typedef BOOL (WINAPI *lpWinHttpReadData)
(HINTERNET hRequest, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
lpWinHttpReadData http_read_data;

typedef BOOL (WINAPI *lpWinHttpCloseHandle)
(HINTERNET hInternet);
lpWinHttpCloseHandle http_close_handle;

typedef BOOL(WINAPI *lpWinHttpQueryHeaders)
(IN     HINTERNET hRequest,
	IN     DWORD     dwInfoLevel,
	IN     LPCWSTR   pwszName OPTIONAL,
	__out_bcount_part_opt(*lpdwBufferLength, *lpdwBufferLength) __out_data_source(NETWORK) LPVOID lpBuffer,
	IN OUT LPDWORD   lpdwBufferLength,
	IN OUT LPDWORD   lpdwIndex OPTIONAL);
lpWinHttpQueryHeaders http_query_header;


void initNetwork()
{
	HMODULE hWinHTTP;
	if (0 == (hWinHTTP = GetModuleHandle(L"winhttp.dll")))
	{
		if (0 == (hWinHTTP = LoadLibrary(L"winhttp.dll")))
		{
			printf("nao achei\n");
		}
	}

	if (hWinHTTP == 0)
	{
		printf("Erro\n");
		return;
	}

	//Monta os ponteiras de funções
	http_open = (lpWinHttpOpen)GetProcAddress(hWinHTTP, "WinHttpOpen");
	http_connect = (lpWinHttpConnect)GetProcAddress(hWinHTTP, "WinHttpConnect");
	http_open_request = (lpWinHttpOpenRequest)GetProcAddress(hWinHTTP, "WinHttpOpenRequest");
	http_send_request = (lpWinHttpSendRequest)GetProcAddress(hWinHTTP, "WinHttpSendRequest");
	http_receive_response = (lpWinHttpReceiveResponse)GetProcAddress(hWinHTTP, "WinHttpReceiveResponse");
	http_query_data = (lpWinHttpQueryDataAvailable)GetProcAddress(hWinHTTP, "WinHttpQueryDataAvailable");
	http_read_data = (lpWinHttpReadData)GetProcAddress(hWinHTTP, "WinHttpReadData");
	http_close_handle = (lpWinHttpCloseHandle)GetProcAddress(hWinHTTP, "WinHttpCloseHandle");
	http_query_header = (lpWinHttpQueryHeaders)GetProcAddress(hWinHTTP, "WinHttpQueryHeaders");
}

inline stdwstring string2wstring(const stdstring str) {
	return stdwstring(str.begin(), str.end());
}

template<class container>
void split_wstring(const wchar_t* str, const wchar_t token, container& ctn)
{
	std::wstring strtoken(1, token);
	wchar_t *next_token = fl_null;
	wchar_t* pch = wcstok_s((wchar_t*)str, strtoken.c_str(), &next_token);
	while (pch != 0)
	{
		std::inserter(ctn, ctn.end()) = pch;
		pch = wcstok_s(0, strtoken.c_str(), &next_token);
	}
}

p_wstring exe_full_path()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	p_wstring ret = std::make_shared<std::wstring>(buffer);
	return ret;
}

p_wstring exe_path()
{
	auto& exe_path = exe_full_path();
	auto& parent_path = fs::path(exe_path->c_str());
	p_wstring ret = MAKE_PWSTRING;
	ret.get()->append(parent_path.parent_path().c_str());
	return ret;
}

struct download_response_data_t {
	data_response_t* data;
	int							 current_size;
};

static size_t curl_write_call_back(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	download_response_data_t* data_response = (download_response_data_t *)userp;

	std::vector<char>& buff = (data_response->data->buffer_cache);

	buff.insert(buff.end(), (char*)contents, (char*)contents + realsize);

	if (data_response->data->res_hwnd)
		SendMessage(data_response->data->res_hwnd, MSG_PROGRESS_DOWNLOAD, data_response->current_size, buff.size());

	return realsize;
}

bool get_file_size(const char* sRequest, int& size)
{
#if USE_CURL
	bool ret = true;
	curl_global_init(CURL_GLOBAL_ALL);
	CURL *curl = curl_easy_init();
	
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, sRequest);

		char error_buffer[CURL_ERROR_SIZE] = {};
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_HEADER, 1);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

		if (!curl_easy_perform(curl)) {
			curl_off_t cl;
			CURLcode res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
			if (!res) {
				size = (int)cl;
				ret = true;
			}
			else
				ret = false;
		}
		else
			ret = false;

		curl_easy_cleanup(curl);
	}

	return ret;
#else
	bool ret = false;
	DWORD dwSize = 0;
	//LPVOID lpOutBuffer = NULL;
	BOOL  bResults = FALSE;
	HINTERNET hSession = NULL,
		hConnect = NULL,
		hRequest = NULL;

	// Use WinHttpOpen to obtain a session handle.
	hSession = http_open(L"MyQ EVKey Updater/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);

	stdwstring strlink = string2wstring(sRequest);

	if (hSession)
		hConnect = http_connect(hSession, g_server,
			INTERNET_DEFAULT_HTTPS_PORT, 0);

	if (hConnect)
		hRequest = http_open_request(hConnect, L"GET", (LPCWSTR)strlink.c_str(),
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);

	if (hRequest)
		bResults = http_send_request(hRequest,
			WINHTTP_NO_ADDITIONAL_HEADERS,
			0, WINHTTP_NO_REQUEST_DATA, 0,
			0, 0);

	if (bResults)
		bResults = http_receive_response(hRequest, NULL);

	if (bResults)
	{
		DWORD _len = 0xdeadbeef;
		DWORD _size = sizeof(_len);
		if(http_query_header(hRequest, 5 | 0x20000000,
			NULL, &_len,	&_size, NULL))
		{
			size = _len;
			ret = true;
		}
	}

	if (hRequest) http_close_handle(hRequest);
	if (hConnect) http_close_handle(hConnect);
	if (hSession) http_close_handle(hSession);

	return ret;
#endif
}

namespace sp_updater
{
	BOOL send_request(const char* sRequest, data_response_t& sResponse)
	{
		sResponse.buffer_cache.resize(0);
#if USE_CURL
		curl_global_init(CURL_GLOBAL_ALL);
		CURL * curl = curl_easy_init();
		BOOL ret = FALSE;
		if (curl)
		{
			int size;
			if (get_file_size(sRequest, size))
			{
				download_response_data_t rdata = { &sResponse, (int)size };

				curl_easy_setopt(curl, CURLOPT_URL, sRequest);

				char error_buffer[CURL_ERROR_SIZE] = {};
				curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_call_back);


				curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
				//curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");

				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rdata);

				ret = curl_easy_perform(curl) == CURLcode::CURLE_OK;
			}
			curl_easy_cleanup(curl);
		}
#else
		BOOL ret = 1;
		int file_zize;
		if (get_file_size(sRequest, file_zize))
		{
			DWORD dwSize = 0;
			DWORD dwDownloaded = 0;
			bool  bResults = false;
			HINTERNET   hSession = NULL,
				hConnect = NULL,
				hRequest = NULL;
			char texto[256];
			DWORD total = 0;
			DWORD totalACarregar = 0;
			time_t initTime;
			time_t atualTime;
			DWORD secLoaded = 0;

			stdwstring strlink = string2wstring(sRequest);

			hSession = http_open(L"MyQ EVKey Updater/1.0",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME,
				WINHTTP_NO_PROXY_BYPASS, 0);
			// Specify an HTTP server.
			if (hSession)
				hConnect = http_connect(hSession, g_server,
					INTERNET_DEFAULT_HTTPS_PORT, 0);
			// Create an HTTP request handle.
			if (hConnect)
				hRequest = http_open_request(hConnect, L"GET", (LPCWSTR)strlink.c_str(),
					NULL, WINHTTP_NO_REFERER,
					WINHTTP_DEFAULT_ACCEPT_TYPES,
					WINHTTP_FLAG_SECURE);
			// Send a request.
			if (hRequest)
				bResults = http_send_request(hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0,
					0, 0);
			// End the request.
			if (bResults)
				bResults = http_receive_response(hRequest, NULL);

			// Keep checking for data until there is nothing left.
			initTime = 0;//time(0);
			if (bResults)
			{
				do
				{
					// Check for available data.
					dwSize = 0;
					if (!http_query_data(hRequest, &dwSize))
					{
						int _err = GetLastError();
						ret = 0;
						break;
					}

					if (dwSize == 0)
						break;

					if (sResponse.res_hwnd)
						SendMessage(sResponse.res_hwnd, MSG_PROGRESS_DOWNLOAD, 1000000, dwSize);

					// Allocate space for the buffer.
					std::vector<char> buffer(dwSize + 1);
					ZeroMemory(buffer.data(), dwSize + 1);

					if (!http_read_data(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded))
					{
						int _err = GetLastError();
						ret = 0;
						break;
					}

					sResponse.buffer_cache.insert(sResponse.buffer_cache.end(), buffer.data(), buffer.data() + dwSize);

				} while (dwSize > 0);
			}
			else
			{
				if (sResponse.res_hwnd)
					SendMessage(sResponse.res_hwnd, MSG_PROGRESS_DOWNLOAD, -1, 0);
			}

			if (hRequest) http_close_handle(hRequest);
			if (hConnect) http_close_handle(hConnect);
			if (hSession) http_close_handle(hSession);
		}

#endif

		if (sResponse.res_hwnd)
		{
			if (ret)
				SendMessage(sResponse.res_hwnd, MSG_PROGRESS_DOWNLOAD, 100, 100);
			else
				SendMessage(sResponse.res_hwnd, MSG_PROGRESS_DOWNLOAD, -1, 0);
		}

		return ret;
	}


	int process_command(LPWSTR lpCmdLine, int& out_version, int& out_flat_form)
	{
		if (lpCmdLine && wcslen(lpCmdLine))
		{
			std::vector<stdwstring> cmds;
			split_wstring(lpCmdLine, L' ', cmds);

			if (cmds.size() < 2)
				return error_command;

			int flatform = _wtoi(cmds[0].c_str());
			int version = _wtoi(cmds[1].c_str());

			if (flatform != 32 && flatform != 64)
				return error_command;

			if (version < 100 && version > 900)
				return error_command;

			out_flat_form = flatform;

			data_response_t data;

			std::string link = std::string(g_link) + "cversion.txt";
			if (send_request(link.c_str(), data))
			{
				std::string str_new_version(data.buffer_cache.begin(), data.buffer_cache.end());
				out_version = atoi(str_new_version.c_str());

				if (out_version > version)
					return out_version;
				else
					return error_noerror;
			}
			else
				return error_connection;
		}
		else
		{

		}

		return error_command;
	}

	int action(HINSTANCE hins, HWND main_hwnd, LPWSTR lpCmdLine, bool is_silence)
	{
		initNetwork();

		int version, flatform;
		int ret = process_command(lpCmdLine, version, flatform);

		if (ret > error_noerror)
		{
			int l = (version / 100);
			int m = (version / 10) % 10;
			int s = version % 10;
			stdwstring strversion = std::to_wstring(l) + L"." + std::to_wstring(m) + L"." + std::to_wstring(s);
			stdwstring strinform = L"EVKey có version " + strversion + L" mới. Bạn có muốn update không ?";

			if (MessageBox(main_hwnd, strinform.c_str(), L"EVKey - Auto update", MB_YESNO | MB_ICONINFORMATION) == IDYES)
			{
				std::string file_name = "EVKey" + std::to_string(flatform) + ".exe";
				std::string link = std::string(g_link) + file_name;
				c_download_dlg dlg(link, hins, main_hwnd);
				dlg.ShowDialog();

				if (dlg.m_downloaded_size == dlg.m_data_size)
				{
					stdwstring fname = *exe_path().get() + L"\\EVKey" + std::to_wstring(flatform) + L".exe";

					FILE* file;
					_wfopen_s(&file, fname.c_str(), L"wb");
					if (file)
					{
						int size = fwrite(dlg.m_down_data.buffer_cache.data(),
							dlg.m_down_data.buffer_cache.size(),
							1, file);

						if (size != 1 && !is_silence)
							MessageBoxW(NULL, L"Không thể ghi file. Vui lòng kiểm tra lại.", L"EVKey - Auto update", MB_ICONERROR);

						fclose(file);
					}
					else
					{
						if (!is_silence)
							MessageBoxW(NULL, L"Không thể ghi file. Vui lòng kiểm tra lại.", L"EVKey - Auto update", MB_ICONERROR);
					}

					ret = error_download_success;
				}
				else
				{
					ret = error_connection;
				}
			}
		}

		switch (ret)
		{
		case error_noerror:
		{
			if (!is_silence)
				MessageBoxW(NULL, L"Version đang sử dụng là mới nhất.", L"EVKey - Auto update", MB_ICONINFORMATION);
		}
		break;

		case error_connection:
		{
			if (!is_silence)
				MessageBoxW(NULL, L"Lỗi không thể kết nối tới máy chủ.", L"EVKey - Auto update", MB_ICONERROR);
		}
		break;

		case error_command:
		{
			if (!is_silence)
				MessageBoxW(NULL, L"Có lỗi trong quá trình thực thi.", L"EVKey - Auto update", MB_ICONERROR);
		}
		break;

		default:
			break;
		}

		DETROY_WINDOW(main_hwnd);

		return ret;
	}
}