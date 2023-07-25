#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

using namespace std;

bool fileExists(const string& filePath) {
    return (access(filePath.c_str(), F_OK) == 0);
}
string executeCommand(const string& command) {
    string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

vector<string> getFilenamesInDirectory(const string& directoryPath) {
    vector<string> filenames;

    DIR* directory = opendir(directoryPath.c_str());
    if (directory) {
        dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            string filename = entry->d_name;
            // Exclude "." and ".." entries
            if (filename != "." && filename != "..") {
                filenames.push_back(filename);
            }
        }
        closedir(directory);

        // Sort filenames in alphabetical order
        sort(filenames.begin(), filenames.end());
    } else {
        cout << "Failed to open directory." << endl;
    }

    return filenames;
}

int main() {
    string computerMusicPath = string(getenv("HOME")) + "/Music";
    vector<string> computerFilenames = getFilenamesInDirectory(computerMusicPath);


    // Use ADB to get the list of files in the phone music folder
    string phoneMusicPath = "/storage/emulated/0/music";
    string adbCommand = "adb shell \"cd " + phoneMusicPath + " && stat -c %Y%n *.*\"";
    string phoneMusicFiles = executeCommand(adbCommand);

    vector<string> phoneFilenames;
    vector<int> phoneTimestamps;

    cout << "\n________________________________________________________________________________\n";
    cout << "PHONE TO COMPUTER\n";
    cout << "--------------------------------------------------------------------------------\n\n";

    size_t pos = 0;
    while ((pos = phoneMusicFiles.find('\n')) != string::npos) {
        string phoneFile = phoneMusicFiles.substr(0, pos);
        string filename = phoneFile.substr(10);
        phoneFilenames.push_back(filename); // Add filename to the vector
        phoneTimestamps.push_back(stoi(phoneFile.substr(0,10)));
        phoneMusicFiles.erase(0, pos + 1);

        // Construct the full path of the phone file
        string phoneFilePath = phoneMusicPath + "/" + filename;

        // Check if the phone file exists in the computer folder
        if (!fileExists(computerMusicPath + "/" + filename)) {
            // If the phone file doesn't exist on the computer, copy it
            string copyCommand = "adb pull -a \"" + phoneFilePath + "\" \"" + computerMusicPath + "\"";
            system(copyCommand.c_str());
        } else {
            // If the phone file exists on the computer, compare the modified dates
            struct stat computerStat;
            string computerFilePath = computerMusicPath + "/" + filename;

            if (stat(computerFilePath.c_str(), &computerStat) == 0) {
                int phoneModified = stoi(phoneFile.substr(0,10));
                int computerModified = static_cast<int>(computerStat.st_mtime);
                if (phoneModified-10 > computerModified) {
                    // If the phone file is newer, copy it to the computer
                    string copyCommand = "adb pull -a \"" + phoneFilePath + "\" \"" + computerMusicPath + "\"";
                    system(copyCommand.c_str());
                }
            }
        }
    }
    
    cout << "\n________________________________________________________________________________\n";
    cout << "COMPUTER TO PHONE\n";
    cout << "--------------------------------------------------------------------------------\n\n";
    size_t index;
    for (const auto& computerFile : computerFilenames) {
        //Fails when you have a bunch on the phone and not the computer.
        auto it = find(phoneFilenames.begin(), phoneFilenames.end(), computerFile);
        if (it != phoneFilenames.end()) { //If computer file is on the phone
            index = distance(phoneFilenames.begin(), it);
            struct stat computerStat;
            string computerFilePath = computerMusicPath + "/" + computerFile;

            if (stat(computerFilePath.c_str(), &computerStat) == 0) {
                int computerModified = static_cast<int>(computerStat.st_mtime);
                if (computerModified > phoneTimestamps[index] + 10) {
                    // If the computer file is newer, copy it to the phone
                    string copyCommand = "adb push \"" + computerMusicPath + "/" + computerFile + "\" \"" + phoneMusicPath + "\"";
                    system(copyCommand.c_str());
                }
            }
         
        } else {
            // If the computer file doesn't exist on the phone, copy it
            string copyCommand = "adb push \"" + computerMusicPath + "/" + computerFile + "\" \"" + phoneMusicPath + "\"";
            system(copyCommand.c_str());
        }
    }

    cout << "\nIt is now safe to unplug your device." << endl;

    return 0;
}

//Can be 20% faster with multithreading, if I could make that work.
