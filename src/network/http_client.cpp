// src/network/http_client.cpp
#include "network/http_client.h"
#include "jpm_config.h"

#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {

/*========  helpers  ========*/
size_t write_to_string(void* contents, size_t size, size_t nmemb, void* userp)
{
    const std::size_t realSize = size * nmemb;
    auto* target = static_cast<std::string*>(userp);
    target->append(static_cast<char*>(contents), realSize);
    return realSize;
}

size_t write_to_stream(void* contents, size_t size, size_t nmemb, void* userp)
{
    const std::size_t realSize = size * nmemb;
    auto* out = static_cast<std::ostream*>(userp);
    out->write(static_cast<char*>(contents), realSize);
    return out->good() ? realSize : 0;        // abort transfer if write fails
}

} // namespace

namespace jpm {

HttpClient::HttpClient()
{
    if (g_verbose_output)
        std::cout << "HttpClient initialized (libcurl backend)." << std::endl;
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() { curl_global_cleanup(); }

/*---------------------------------------------------------
 | GET (returns body or std::nullopt on failure)
 *--------------------------------------------------------*/
std::optional<std::string> HttpClient::get(const std::string& url)
{
    if (g_verbose_output)
        std::cout << "HttpClient::get fetching " << url << std::endl;

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "curl_easy_init() failed" << std::endl;
        return std::nullopt;
    }

    std::string buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // follow redirects

    CURLcode res           = curl_easy_perform(curl);
    long      status_code  = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    curl_easy_cleanup(curl);

    if (res == CURLE_OK && status_code == 200) {
        if (g_verbose_output)
            std::cout << "  Success (" << status_code << ")" << std::endl;
        return buffer;
    }

    std::cerr << "HttpClient::get failed (" << res << ", status " << status_code
              << "): " << curl_easy_strerror(res) << std::endl;
    return std::nullopt;
}

/*---------------------------------------------------------
 | download_file (returns true on success)
 *--------------------------------------------------------*/
bool HttpClient::download_file(const std::string& url, const std::string& output_path)
{
    if (g_verbose_output)
        std::cout << "HttpClient::download_file " << url << " → " << output_path
                  << std::endl;

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "  Cannot open " << output_path << " for writing.\n";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "curl_easy_init() failed" << std::endl;
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_stream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res          = curl_easy_perform(curl);
    long     status_code  = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

    curl_easy_cleanup(curl);
    out.close();

    if (res == CURLE_OK && status_code == 200) {
        if (g_verbose_output) {
            struct stat st {};
            if (stat(output_path.c_str(), &st) == 0)
                std::cout << "  Downloaded " << st.st_size << " bytes.\n";
        }
        return true;
    }

    std::cerr << "HttpClient::download_file failed (" << res << ", status "
              << status_code << "): " << curl_easy_strerror(res) << std::endl;
    std::remove(output_path.c_str());
    return false;
}

} // namespace jpm
