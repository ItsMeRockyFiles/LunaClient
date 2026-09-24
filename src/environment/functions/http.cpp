#include "../environment.h"
#include "../../lua/lua.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

// ─────────────────────────────────────────────────────────────────────────────
//  UNC HTTP Functions
//  UNC spec: request(options: table) → response: table
//  Options table fields:
//    Url: string (required)
//    Method: string (default "GET")
//    Headers: table (optional)
//    Body: string (optional)
//  Response table fields:
//    Success: boolean
//    StatusCode: number
//    StatusMessage: string
//    Headers: table
//    Body: string
// ─────────────────────────────────────────────────────────────────────────────

struct HttpResponse {
    bool        success     = false;
    int         status_code = 0;
    std::string status_msg;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

// ── Simple WinHTTP synchronous request ───────────────────────────────────────
static HttpResponse do_http_request(
    const std::string& url,
    const std::string& method,
    const std::string& body,
    const std::vector<std::pair<std::string,std::string>>& req_headers)
{
    HttpResponse resp;

    // Parse URL
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t scheme[32]   = {};
    wchar_t host[256]    = {};
    wchar_t path[2048]   = {};
    uc.lpszScheme        = scheme;  uc.dwSchemeLength    = 32;
    uc.lpszHostName      = host;    uc.dwHostNameLength  = 256;
    uc.lpszUrlPath       = path;    uc.dwUrlPathLength   = 2048;

    std::wstring wurl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        resp.status_msg = "Failed to parse URL";
        return resp;
    }

    bool is_https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = uc.nPort ? uc.nPort : (is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);

    HINTERNET hSession = WinHttpOpen(
        L"LunaClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { resp.status_msg = "WinHttpOpen failed"; return resp; }

    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); resp.status_msg = "WinHttpConnect failed"; return resp; }

    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wmethod(method.begin(), method.end());
    std::wstring wpath(path);

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wmethod.c_str(), wpath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.status_msg = "WinHttpOpenRequest failed";
        return resp;
    }

    // Add custom headers
    for (auto& [k, v] : req_headers) {
        std::wstring header = std::wstring(k.begin(), k.end()) + L": " + std::wstring(v.begin(), v.end()) + L"\r\n";
        WinHttpAddRequestHeaders(hRequest, header.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send
    const void* body_ptr = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
    DWORD body_len = static_cast<DWORD>(body.size());

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, const_cast<void*>(body_ptr), body_len, body_len, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.status_msg = "WinHttpSendRequest failed";
        return resp;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.status_msg = "WinHttpReceiveResponse failed";
        return resp;
    }

    // Status code
    DWORD status_code = 0, size = sizeof(DWORD);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size, WINHTTP_NO_HEADER_INDEX);
    resp.status_code = static_cast<int>(status_code);
    resp.success = (status_code >= 200 && status_code < 300);

    // Read body
    std::string result_body;
    DWORD bytes_available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
        std::vector<char> buf(bytes_available + 1);
        DWORD bytes_read = 0;
        if (WinHttpReadData(hRequest, buf.data(), bytes_available, &bytes_read)) {
            result_body.append(buf.data(), bytes_read);
        }
    }
    resp.body = std::move(result_body);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}

// ── request(options: table) → response: table ─────────────────────────────────
static int luna_request(lua_State* L) {
    if (!lua.istable(L, 1)) {
        lua.pushstring(L, "request: expected table");
        lua.error(L);
    }

    // Read options
    std::string url, method = "GET", body;
    std::vector<std::pair<std::string,std::string>> req_headers;

    // Url
    lua.pushstring(L, "Url");
    lua.gettable(L, 1);
    if (!lua.isstring(L, -1)) { lua.pushstring(L, "request: Url is required"); lua.error(L); }
    url = lua.tostring(L, -1);
    lua.pop(L, 1);

    // Method
    lua.pushstring(L, "Method");
    lua.gettable(L, 1);
    if (lua.isstring(L, -1)) method = lua.tostring(L, -1);
    lua.pop(L, 1);

    // Body
    lua.pushstring(L, "Body");
    lua.gettable(L, 1);
    if (lua.isstring(L, -1)) {
        size_t len = 0;
        const char* d = lua.tolstring(L, -1, &len);
        body.assign(d, len);
    }
    lua.pop(L, 1);

    // Headers table
    lua.pushstring(L, "Headers");
    lua.gettable(L, 1);
    if (lua.istable(L, -1)) {
        lua.pushnil(L);
        while (lua.next(L, -2)) {
            if (lua.isstring(L, -2) && lua.isstring(L, -1)) {
                req_headers.emplace_back(lua.tostring(L, -2), lua.tostring(L, -1));
            }
            lua.pop(L, 1);
        }
    }
    lua.pop(L, 1);

    // Execute
    HttpResponse resp = do_http_request(url, method, body, req_headers);

    // Build response table
    lua.createtable(L, 0, 5);

    lua.pushstring(L, "Success");
    lua.pushboolean(L, resp.success ? 1 : 0);
    lua.settable(L, -3);

    lua.pushstring(L, "StatusCode");
    lua.pushnumber(L, static_cast<double>(resp.status_code));
    lua.settable(L, -3);

    lua.pushstring(L, "StatusMessage");
    lua.pushstring(L, resp.status_msg.empty() ? (resp.success ? "OK" : "Error") : resp.status_msg.c_str());
    lua.settable(L, -3);

    lua.pushstring(L, "Body");
    lua.pushlstring(L, resp.body.data(), resp.body.size());
    lua.settable(L, -3);

    lua.pushstring(L, "Headers");
    lua.createtable(L, 0, 0);
    lua.settable(L, -3);

    return 1;
}

namespace environment {

void register_http(lua_State* L) {
    lua.register_fn(L, "request", luna_request);

    // http.request alias (UNC)
    lua.getglobal(L, "http");
    if (!lua.istable(L, -1)) {
        lua.pop(L, 1);
        lua.createtable(L, 0, 0);
    }
    lua.pushstring(L, "request");
    lua.pushcclosure(L, luna_request, "http.request", 0);
    lua.settable(L, -3);
    lua.setglobal(L, "http");

    // syn.request alias (compatibility)
    lua.getglobal(L, "syn");
    if (!lua.istable(L, -1)) {
        lua.pop(L, 1);
        lua.createtable(L, 0, 0);
    }
    lua.pushstring(L, "request");
    lua.pushcclosure(L, luna_request, "syn.request", 0);
    lua.settable(L, -3);
    lua.setglobal(L, "syn");
}

} // namespace environment
