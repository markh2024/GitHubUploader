#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <atomic>
#include <thread>
#include <mutex>

namespace fs = std::filesystem;

class GitHubUploader {
public:
    GitHubUploader();


	bool endsWith(const std::string& str, const std::string& suffix);
	
    // Configuration
    void setRepo(const std::string& repo);
    void setBranch(const std::string& branch);
    void setCommitMessage(const std::string& msg);
    bool loadTokenFromFile(const std::string& tokenFile);

    // Persistence
    void saveSessionConfig();
    void loadSessionConfig();

    // Uploads
    void uploadFile(const std::string& localPath, const std::string& pathInRepo);
    void uploadFolder(const std::string& localFolder, const std::string& baseRepoPath);
    void uploadFolderIfChanged(const std::string& localFolder, const std::string& baseRepoPath);


	
    
private:
    std::string token_;
    std::string repo_;
    std::string branch_;
    std::string commitMsg_;
    nlohmann::json hash_db_;
    
    // Progress display state
    std::string currentFile_;
    int currentIndex_ = 0;
    int totalFiles_ = 0;
    std::string hashFile_ = "data/hash_db.json";
    std::string configFile_ = "data/config.json";

    // Progress display components
    std::atomic<bool> progressActive_{false};
    std::thread progressThread_;
    std::mutex progressMutex_;

    // Helper methods
    std::string sanitizeRepoPath(const std::string& basePath);
    std::string sha256File(const std::string& filePath);
    std::string base64Encode(const std::string& input);
    std::string getFileSHA(const std::string& pathInRepo);
    bool putFileToGitHub(const std::string& filePath, const std::string& pathInRepo);

    // Hash tracking
    void loadHashDB();
    void saveHashDB();
    
    // Progress display methods
    void startProgress();
    void updateProgress(const std::string& fileName, int index, int total);
    void stopProgress();
};
