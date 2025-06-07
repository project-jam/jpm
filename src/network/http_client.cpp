#include "network/http_client.h"
#include "jpm_config.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <sys/stat.h>
#include <cpr/cpr.h>

namespace jpm {

HttpClient::HttpClient() {
    if (g_verbose_output) {
        std::cout << "HttpClient initialized." << std::endl;
    }
}

std::optional<std::string> HttpClient::get(const std::string& url) {
    if (g_verbose_output) {
        std::cout << "HttpClient::get attempting to fetch URL: " << url << std::endl;
    }
    cpr::Response r = cpr::Get(cpr::Url{url});

    if (r.status_code == 200) {
        if (g_verbose_output) {
            std::cout << "HttpClient::get successfully fetched URL: " << url << " with status: " << r.status_code << std::endl;
        }
        return r.text;
    } else if (r.error) {
        std::cerr << "HttpClient::get CPR error for URL " << url << ": " << r.error.message << std::endl;
        return std::nullopt;
    } else {
        std::cerr << "HttpClient::get failed to fetch URL: " << url << " - Status code: " << r.status_code << std::endl;
        if (!r.text.empty()) {
            std::cerr << "Response body (partial or error): " << r.text << std::endl;
        }
        return std::nullopt;
    }
}

bool HttpClient::download_file(const std::string& url, const std::string& output_path) {
    if (g_verbose_output) {
        std::cout << "HttpClient::download_file attempting to download URL: " << url << " to Path: " << output_path << std::endl;
    }

    std::ofstream out_file(output_path, std::ios::binary);
    if (!out_file) {
        std::cerr << "HttpClient::download_file error: Could not open file for writing: " << output_path << std::endl;
        return false;
    }

    cpr::Response r = cpr::Download(out_file, cpr::Url{url});
    out_file.close();

    if (r.status_code == 200 && !r.error) {
        if (g_verbose_output) {
            std::cout << "HttpClient::download_file successfully downloaded " << url << " to " << output_path << std::endl;
            struct stat stat_buf;
            if (stat(output_path.c_str(), &stat_buf) == 0) {
                std::cout << "  Downloaded file size: " << stat_buf.st_size << " bytes." << std::endl;
                if (stat_buf.st_size == 0 && r.downloaded_bytes == 0) {
                    auto it_cl = r.header.find("content-length");
                    if (it_cl != r.header.end() && !it_cl->second.empty() && std::stoi(it_cl->second) > 0) {
                        std::cerr << "  Warning: Downloaded file is 0 bytes but Content-Length header was > 0." << std::endl;
                    }
                }
            } else {
                std::cerr << "  Warning: Could not stat downloaded file: " << output_path << std::endl;
            }
        }
        return true;
    } else if (r.error) {
        std::cerr << "HttpClient::download_file CPR error for URL " << url << ": " << r.error.message << std::endl;
        remove(output_path.c_str());
        return false;
    } else {
        std::cerr << "HttpClient::download_file failed for URL " << url << " - Status code: " << r.status_code << std::endl;
        if (!r.text.empty()) {
            std::cerr << "  Response/Error details: " << r.text << std::endl;
        }
        remove(output_path.c_str());
        return false;
    }
}

} // namespace jpm
