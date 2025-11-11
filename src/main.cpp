#include <iostream>
#include <string>
#include "GitHubUploader.hpp"
#include <thread>

// Typewriter effect
void typeWriter(const std::string& text, int delay_ms = 20, const std::string& color = "\033[97m") {
    std::cout << color;
    for (char c : text) {
        std::cout << c << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    std::cout << "\033[0m\n"; // reset color
}

// Display menu
void showMenu() {
    typeWriter("\n=== GitHub Uploader Menu ===", 10, "\033[96m"); // Bright Cyan
    typeWriter("1. Load GitHub Token from file", 10, "\033[92m");  // Bright Green
    typeWriter("2. Set Repository (user/repo)", 10, "\033[93m");  // Bright Yellow
    typeWriter("3. Set Branch", 10, "\033[95m");  // Bright Magenta
    typeWriter("4. Set Commit Message", 10, "\033[96m");  // Bright Cyan
    typeWriter("5. Upload a File", 10, "\033[92m");  // Bright Green
    typeWriter("6. Upload a Folder/Project (full)", 10, "\033[93m");  // Bright Yellow
    typeWriter("7. Upload Folder (only changed files)", 10, "\033[96m");  // Bright Cyan
    typeWriter("0. Exit", 10, "\033[91m");  // Bright Red
    std::cout << "Select an option: ";
}

int main() {
    GitHubUploader uploader;
    uploader.loadSessionConfig();  // Load last used settings
    int choice;

    do {
        showMenu();
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // flush input

        switch(choice) {
            case 1: {
                std::string tokenFile = "/home/mark/.github_token/githubtoken.dat";
                if (uploader.loadTokenFromFile(tokenFile)) {
                    typeWriter("Token loaded successfully!", 10, "\033[92m");
                } else {
                    typeWriter("Failed to load token.", 10, "\033[91m");
                }
                break;
            }
            case 2: {
                std::string repo;
                std::cout << "Enter Repository (user/repo): ";
                std::getline(std::cin, repo);
                uploader.setRepo(repo);
                break;
            }
            case 3: {
                std::string branch;
                std::cout << "Enter Branch: ";
                std::getline(std::cin, branch);
                uploader.setBranch(branch);
                break;
            }
            case 4: {
                std::string msg;
                std::cout << "Enter Commit Message: ";
                std::getline(std::cin, msg);
                uploader.setCommitMessage(msg);
                break;
            }
            case 5: {
                std::string filePath;
                std::cout << "Enter file path to upload: ";
                std::getline(std::cin, filePath);
                std::string repoPath = ""; // root by default
                uploader.uploadFile(filePath, repoPath);
                break;
            }
            case 6: {
                std::string folder;
                std::cout << "Enter folder path to upload: ";
                std::getline(std::cin, folder);
                std::string baseRepoPath = "."; // default to root
                uploader.uploadFolder(folder, baseRepoPath);
                break;
            }
            case 7: {
                std::string folder;
                std::cout << "Enter folder path for incremental upload: ";
                std::getline(std::cin, folder);
                std::string baseRepoPath = "."; // default to root
                uploader.uploadFolderIfChanged(folder, baseRepoPath);
                break;
            }
           case 0:
				uploader.saveSessionConfig();
				typeWriter("Session saved. Goodbye!", 10, "\033[91m");
				break;
				
            default:
                typeWriter("Invalid option. Try again.", 10, "\033[91m");
        }

    } while (choice != 0);

    return 0;
}
