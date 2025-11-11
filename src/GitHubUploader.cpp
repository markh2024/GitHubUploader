#include "GitHubUploader.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <openssl/evp.h>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <cstring>  

namespace fs = std::filesystem;




// === Basic Setters ===
void GitHubUploader::setRepo(const std::string& repo) { repo_ = repo; }
void GitHubUploader::setBranch(const std::string& branch) { branch_ = branch; }
void GitHubUploader::setCommitMessage(const std::string& msg) { commitMsg_ = msg; }

GitHubUploader::GitHubUploader() {
    loadHashDB();
}


bool GitHubUploader::endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}



// === Token Handling ===
bool GitHubUploader::loadTokenFromFile(const std::string& tokenFile) {
    std::ifstream in(tokenFile);
    if (!in.is_open()) return false;
    std::getline(in, token_);
    return true;
}

// Callback function for CURL to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Base64 encoding helper
std::string GitHubUploader::base64Encode(const std::string& input) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.size();
    const unsigned char* bytes_to_encode = (const unsigned char*)input.c_str();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
            ret += '=';
    }

    return ret;
}

// Get the SHA of an existing file from GitHub (needed for updates)
std::string GitHubUploader::getFileSHA(const std::string& pathInRepo) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string readBuffer;
    std::string url = "https://api.github.com/repos/" + repo_ + "/contents/" + pathInRepo;
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "User-Agent: GitHubUploader");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";

    // Parse JSON response to get SHA
    try {
        auto response = nlohmann::json::parse(readBuffer);
        if (response.contains("sha")) {
            return response["sha"].get<std::string>();
        }
    } catch (...) {
        // File doesn't exist or parse error
    }

    return "";
}

// Main GitHub upload function

bool GitHubUploader::putFileToGitHub(const std::string& filePath, const std::string& pathInRepo) {
    // Normalize repo path (remove leading slashes)
    std::string normalizedPath = pathInRepo;
    while (!normalizedPath.empty() && normalizedPath.front() == '/')
        normalizedPath.erase(0, 1);

    // Read file content
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filePath << std::endl;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Base64 encode the content
    std::string base64Content = base64Encode(content);

    // Check if file exists to get its SHA (needed for updates)
    std::string existingSHA = getFileSHA(normalizedPath);

    // Prepare JSON payload
    nlohmann::json payload;
    payload["message"] = commitMsg_;
    payload["content"] = base64Content;
    payload["branch"] = branch_;
    
    if (!existingSHA.empty()) {
        payload["sha"] = existingSHA;  // Required for updating existing files
    }

    std::string jsonPayload = payload.dump();

    // Initialize CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error: Failed to initialize CURL" << std::endl;
        return false;
    }

    std::string readBuffer;
    std::string url = "https://api.github.com/repos/" + repo_ + "/contents/" + normalizedPath;

    // Set up headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: GitHubUploader");

    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // Check result
    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    if (response_code == 200 || response_code == 201) {
        return true;  // Success
    } 
    else if (response_code == 404) {
        std::cerr << "GitHub repository or path not found:\n"
                  << "Repo: " << repo_ << "\n"
                  << "Path: " << normalizedPath << "\n"
                  << "Response: " << readBuffer << std::endl;
        return false;
    } 
    else {
        std::cerr << "GitHub API error (HTTP " << response_code << "): " << readBuffer << std::endl;
        return false;
    }
}



// === Path Cleanup ===
std::string GitHubUploader::sanitizeRepoPath(const std::string& basePath) {
    if (basePath.empty() || basePath == ".") return "";
    std::string path = basePath;
    std::replace(path.begin(), path.end(), '\\', '/');
    if (!path.empty() && path.front() == '/') path.erase(0,1);
    if (!path.empty() && path.back() == '/') path.pop_back();
    return path;
}

// === Spinner Thread ===
void GitHubUploader::startProgress() {
    if (progressActive_) return;
    progressActive_ = true;

    progressThread_ = std::thread([this]() {
        static const char spinner[] = {'|', '/', '-', '\\'};
        int i = 0;
        while (progressActive_) {
            {
                std::lock_guard<std::mutex> lock(progressMutex_);
                std::cout << "\rUploading (" << currentIndex_ << "/" << totalFiles_ << ") "
                          << currentFile_ << "  " << spinner[i % 4] << std::flush;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            i++;
        }
        std::cout << "\rAll uploads complete! ✔️                     " << std::endl;
    });
}

void GitHubUploader::updateProgress(const std::string& fileName, int index, int total) {
    std::lock_guard<std::mutex> lock(progressMutex_);
    currentFile_ = fileName;
    currentIndex_ = index;
    totalFiles_ = total;
}

void GitHubUploader::stopProgress() {
    if (!progressActive_) return;
    progressActive_ = false;
    if (progressThread_.joinable()) progressThread_.join();
}

// === Upload Logic ===
void GitHubUploader::uploadFolder(const std::string& localFolder, const std::string& baseRepoPath) {
    std::string repoPath = sanitizeRepoPath(baseRepoPath);
    std::vector<std::string> files;

    for (auto& p : fs::recursive_directory_iterator(localFolder)) {
        if (p.is_regular_file()) {
            std::string relativePath = fs::relative(p.path(), localFolder).generic_string();
            std::string pathInRepo = repoPath.empty() ? relativePath : repoPath + "/" + relativePath;
            files.push_back(p.path().string());
        }
    }

    if (files.empty()) {
        std::cout << "No files found to upload.\n";
        return;
    }

    startProgress();
    for (size_t i = 0; i < files.size(); ++i) {
        updateProgress(files[i], i + 1, files.size());
        putFileToGitHub(files[i], fs::relative(files[i], localFolder).generic_string());
    }
    stopProgress();
}



void GitHubUploader::uploadFile(const std::string& localPath, const std::string& pathInRepo) {
    if (putFileToGitHub(localPath, pathInRepo)) {
        std::cout << "Uploaded: " << pathInRepo << "\n";
    } else {
        std::cout << "Failed: " << pathInRepo << "\n";
    }
}

// === Config & Hash DB ===
void GitHubUploader::loadHashDB() {
    std::ifstream in(hashFile_);
    if (in.is_open()) in >> hash_db_;
}

void GitHubUploader::saveHashDB() {
    std::ofstream out(hashFile_);
    out << hash_db_.dump(4);
}

void GitHubUploader::saveSessionConfig() {
    nlohmann::json cfg;
    cfg["repo"] = repo_;
    cfg["branch"] = branch_;
    cfg["commit_message"] = commitMsg_;
    std::ofstream out(configFile_);
    if (out.is_open()) out << cfg.dump(4);
}

void GitHubUploader::loadSessionConfig() {
    std::ifstream in(configFile_);
    if (!in.is_open()) return;
    nlohmann::json cfg;
    in >> cfg;
    repo_ = cfg.value("repo", "");
    branch_ = cfg.value("branch", "main");
    commitMsg_ = cfg.value("commit_message", "Updated files");
}

// === SHA256 (EVP Modern API) ===
std::string GitHubUploader::sha256File(const std::string& filePath) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) return "";

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }

    std::ifstream file(filePath, std::ifstream::binary);
    if (!file.is_open()) {
        EVP_MD_CTX_free(context);
        return "";
    }

    char buffer[4096];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        if (EVP_DigestUpdate(context, buffer, file.gcount()) != 1) {
            EVP_MD_CTX_free(context);
            return "";
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;
    if (EVP_DigestFinal_ex(context, hash, &lengthOfHash) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }

    EVP_MD_CTX_free(context);

    std::stringstream ss;
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void GitHubUploader::uploadFolderIfChanged(const std::string& localFolder, const std::string& baseRepoPath) {
    std::cout << "Scanning folder for incremental upload: " << localFolder << std::endl;

    loadHashDB(); // ensure hash DB is loaded

    std::vector<std::string> changedFiles;
    std::vector<std::string> failedFiles;

    // Load exclusion rules from JSON file
    std::vector<std::string> excludeFiles, excludeDirs, excludePatterns;
    std::ifstream exclFile("data/exclude_patterns.json");
    if (exclFile.is_open()) {
        nlohmann::json j;
        exclFile >> j;
        if (j.contains("files")) for (auto& f : j["files"]) excludeFiles.push_back(f.get<std::string>());
        if (j.contains("dirs"))  for (auto& d : j["dirs"])  excludeDirs.push_back(d.get<std::string>());
        if (j.contains("patterns")) for (auto& p : j["patterns"]) excludePatterns.push_back(p.get<std::string>());
        exclFile.close();
    }

    auto isExcludedFile = [&](const std::string& filePath) {
        std::string filename = fs::path(filePath).filename().string();
        // Explicit filename match
        for (const auto& f : excludeFiles)
            if (filename == f) return true;
        // Pattern match
        for (const auto& pat : excludePatterns)
            if (filename.find(pat) != std::string::npos || endsWith(filename, pat)) return true;
        return false;
    };

    auto isExcludedDir = [&](const std::string& path) {
        for (const auto& dir : excludeDirs) {
            std::string dirSlash = "/" + dir;
            if (path.find("/" + dir + "/") != std::string::npos || endsWith(path, dirSlash)) return true;
        }
        return false;
    };

    // Recursive scan of all files
    for (const auto& entry : fs::recursive_directory_iterator(localFolder)) {
        std::string filePath = entry.path().string();

        if (!entry.is_regular_file()) {
            if (isExcludedDir(filePath)) {
                std::cout << "Skipping excluded directory: " << filePath << std::endl;
                continue;
            }
            std::cout << "Skipping (not a regular file): " << filePath << std::endl;
            continue;
        }

        if (isExcludedFile(filePath) || isExcludedDir(filePath)) {
            std::cout << "Skipping secret/excluded file: " << filePath << std::endl;
            continue;
        }

        std::string currentHash = sha256File(filePath);
        auto it = hash_db_.find(filePath);

        if (it == hash_db_.end()) {
            std::cout << "New file detected: " << filePath << std::endl;
            changedFiles.push_back(filePath);
            hash_db_[filePath] = currentHash;
        } else if (it.value() != currentHash) {
            std::cout << "Changed file detected: " << filePath << std::endl;
            changedFiles.push_back(filePath);
            hash_db_[filePath] = currentHash;
        } else {
            std::cout << "File unchanged, skipping: " << filePath << std::endl;
        }
    }

    if (changedFiles.empty()) {
        std::cout << "No new or changed files found. Nothing to upload." << std::endl;
        return;
    }

    // Upload changed files with progress
    startProgress();
    int index = 0;
    int total = static_cast<int>(changedFiles.size());

    for (const auto& file : changedFiles) {
        std::string pathInRepo = baseRepoPath;
        if (!pathInRepo.empty() && pathInRepo.back() != '/')
            pathInRepo += '/';
        pathInRepo += fs::relative(file, localFolder).generic_string(); // preserve folder structure

        std::string filenameOnly = fs::path(file).filename().string();
        updateProgress(filenameOnly, ++index, total);

        if (!putFileToGitHub(file, pathInRepo)) {
            std::cerr << "Warning: Failed to upload " << filenameOnly << " (skipping)" << std::endl;
            failedFiles.push_back(file);
            continue;
        }
    }

    stopProgress();
    saveHashDB();

    // Summary
    std::cout << "Incremental upload complete. " << total << " file(s) processed." << std::endl;
    if (!failedFiles.empty()) {
        std::cout << "Files failed to upload:" << std::endl;
        for (const auto& f : failedFiles) std::cout << "  - " << f << std::endl;
    }
}

