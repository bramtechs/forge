#include <iostream>
#include <vector>
#include <unordered_map>

#ifdef __linux__
    #define LINUX
#elif defined(_WIN32) || defined(WIN32)
    #define WINDOWS
#endif

// =============================
// === PROJECT CONFIGURATION ===
// =============================

#define CHECK_GITIGNORE

std::string RELEASES_DIRECTORY = "releases";
std::string BUILD_DIRECTORY = "build";

// executable file
#ifdef LINUX
const std::string EXECUTABLE_DEBUG = "build/Debug/forge_example.exe";
const std::string EXECUTABLE_RELEASE = "build/Release/forge_example.exe";
#elif defined(WINDOWS)
// TODO:
std::string EXECUTABLE_DEBUG = "build/Debug/forge_example.exe";
std::string EXECUTABLE_RELEASE = "build/Release/forge_example.exe";
#endif

// cmake generator
#ifdef LINUX
std::string CMAKE_GENERATOR = "CodeBlocks - Unix Makefiles";
#elif defined(WINDOWS)
std::string CMAKE_GENERATOR = "Visual Studio 17 2022";
#endif

// additional files/folders that should be packaged in zip (package command)
std::unordered_map<std::string,std::string> ADDITIONAL_FILES = {
    { "src", "source" },
    { "LICENSE.txt", ""}
};

// Necessary tools to build the program (searches in PATH)
const std::vector<std::string> REQUIRED_TOOLS = {
    "cmake",
    "git",
#ifdef LINUX
    "g++",
#elif defined(WINDOWS)
    "cl",
#endif
};

// Online libraries required to build the program (folder + url)
std::vector<std::pair<std::string,std::string>> REQUIRED_REPOS = {
    { "libs/raylib","https://github.com/bramtechs/raylib-lite.git" },
};

// =============================

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <initializer_list>
#include <filesystem>
#include <sstream>
#include <thread>
#include "zip_file.hpp"

#ifdef __linux__
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#include <processthreadsapi.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

namespace fs = std::filesystem;

typedef std::unordered_map<std::string,std::string> Table;

const wchar_t PATH_SEP = fs::path::preferred_separator;

// utility functions
static bool subfiles_exist(std::string path);
static bool make_dirs(std::string path);
static std::string concat_sep(char sep, std::initializer_list<std::string> list);
static bool run_command(std::initializer_list<std::string> list);
static bool delete_recursive(std::string path);
static void print_table(Table table, int padding=3);
static bool has_program(std::string program);

// TODO: linux
bool check(bool verbose=false){
    bool good = true;
    Table table;
    for (const auto& tool : REQUIRED_TOOLS){
        std::string desc;
        if (has_program(tool)){
            desc = "FOUND";
        }else{
            desc = "MISSING";
            good = false;
        }
        table.insert({tool,desc});
    }
    if (!good || verbose){
        print_table(table);
    }
    if (!good){
        std::cout << "You are missing required software!" << std::endl;
    }
    return good;
}

bool check_verbose(){
    return check(true);
}

bool clean(){
    if (fs::exists(BUILD_DIRECTORY)){
        return delete_recursive(BUILD_DIRECTORY);
    }
    return true;
}

bool wipe(){
    std::cout << "Wiping libraries!" << std::endl;
    for (const auto& repo : REQUIRED_REPOS) {
        std::cout << ".. " << repo.first << std::endl;
        if (!delete_recursive(repo.first)){
            return false;
        }
    }
    std::cout << "Wiping build folder!" << std::endl;
    delete_recursive(BUILD_DIRECTORY);
    return true;
}

bool download() {
    // download libraries
    std::cout << "Checking if libraries are present..." << std::endl;
    for (const auto& repo : REQUIRED_REPOS) {
        if (fs::exists(repo.first) && subfiles_exist(repo.first)) {
            std::cout << "Updating repo " << repo.first << std::endl;

            if (!run_command({"git", "fetch", repo.first, "-p"})){
                return false;
            }
            if (!run_command({"cd",repo.first,"&","git", "merge"})){
                return false;
            }

        } else {

            // fail-safe if folder sticked around
            if (fs::exists(repo.first)){ 
                std::cout << "Incomplete repo found! Wiping it..." << std::endl;
                if (!delete_recursive(repo.first)){
                    return false;
                }
            }

            std::cout << "Dowloading repo " << repo.second << std::endl;
            if (!run_command({"git", "clone", repo.second, repo.first, "--recursive"})){
                return false;
            }
        }
    }
    return true;
}

void insert_if_missing(std::stringstream& stream, const std::string& text){
    std::string line;
    while (std::getline(stream,line)){
        if (line == text){
            // doesn't need to be added
            return;
        }
    }
    // add text to stream
    stream << std::endl << text;
    std::cout << "Added " << text << " to .gitignore" << std::endl;
}

#ifdef CHECK_GITIGNORE
void check_gitignore() {
    if (fs::exists(".gitignore")){
        std::cout << "Patching .gitignore..." << std::endl;

        // read the file
        std::ifstream ignoreFile(".gitignore");
        std::string content;
        ignoreFile >> content;
        ignoreFile.close();


        // process file text 
        std::stringstream stream(content);

        insert_if_missing(stream, BUILD_DIRECTORY);
        insert_if_missing(stream, RELEASES_DIRECTORY);

        std::cout << stream.str() << std::endl;

        // write back into file
        std::ofstream ignoreFileOut(".gitignore");
        ignoreFileOut << stream.str();
        ignoreFileOut.close();
    }
}
#endif

bool generate() {
#ifdef CHECK_GITIGNORE
    check_gitignore();
#endif

    if (!download()){
        return false;
    }

    std::cout << "Generating cmake project..." << std::endl;
#ifdef LINUX
    return run_command({"cmake", "-S", ".", "-B", "build", "-G", "\"",CMAKE_GENERATOR,"\"" });
#elif defined(WINDOWS)
    return run_command({"cmake", "-S", ".", "-B", "build", "-G", "\"", CMAKE_GENERATOR,"\"", "-A", "Win32"});
#endif
}

bool build() {
    if (!fs::exists(BUILD_DIRECTORY)){
        if (!generate()){
            return false;
        }
    }

    unsigned int threads = std::thread::hardware_concurrency();
    std::cout << "Building cmake project..." << std::endl;
    if (threads > 0){
        return run_command({"cmake", "--build", "build", "-j", std::to_string(threads), "--config", "Debug"});
    }
    return run_command({"cmake", "--build", "build", "--config", "Debug"});
}

bool release() {
    if (!fs::exists(BUILD_DIRECTORY)){
        if (!generate()){
            return false;
        }
    }

    unsigned int threads = std::thread::hardware_concurrency();
    std::cout << "Building cmake project..." << std::endl;
    if (threads > 0){
        return run_command({"cmake", "--build", "build", "-j", std::to_string(threads), "--config", "Release"});
    }
    return run_command({"cmake", "--build", "build", "--config", "Release"});
}

bool archive_file(miniz_cpp::zip_file& file, std::string source,
                                             std::string dest){
    if (!fs::exists(source)){
        std::cerr << "Could not find file " << source << " to archive!" << std::endl;
        return false;
    }

    // read file bytes
    std::string data;
    try {
        std::ifstream stream(source);
        std::stringstream buffer;
        buffer << stream.rdbuf();
        data = buffer.str();
    } catch (std::exception const& ex){
        std::cerr << "Failed to read file data of " << source << std::endl;
        std::cerr << ex.what() << std::endl;
        return false;
    }

    try {
        // write into archive
        std::cout << data << std::endl;
        file.writestr(dest, data);
    } catch(std::runtime_error const& ex){
        std::cerr << "Failed to write file " << source << " into archive at " << dest << std::endl;
        std::cerr << ex.what() << std::endl;
        return false;
    }

    return true;
}

bool package(){
    std::cout << "Compiling and packaging release build!" << std::endl;

    if (!release()){
        return false;
    }

    if (!make_dirs(RELEASES_DIRECTORY)){
        return false;
    }

    // package all the things
    miniz_cpp::zip_file zipFile;

    auto fileName = fs::path(EXECUTABLE_RELEASE).filename();
    if (!archive_file(zipFile, EXECUTABLE_RELEASE, fileName.string())){
        return false;
    }

    for (const auto& file : ADDITIONAL_FILES){
        std::string dest = file.second.empty() ? file.first:file.second;
        if (!archive_file(zipFile, file.first, dest)){
            return false;
        }
    }

    // save the archive
    std::string savePath = concat_sep(PATH_SEP,{RELEASES_DIRECTORY,"build.zip"});
    zipFile.save(savePath);
    std::cout << "Wrote release archive!" << std::endl;

    return true;
}

bool run(std::string path) {
    auto p = fs::path(path.c_str());
    std::string file = p.filename().string();
    std::string folder = p.parent_path().string();

    if (fs::exists(path)) {
        // TODO: might not work on linux
        return run_command({"cd", folder, "&", file });
    }

    std::cerr << "Executable " << path << " does not exist!" << std::endl;
    return false;
}

bool rundebug() {
    if (!build()){
        return false;
    }
    return run(EXECUTABLE_DEBUG);
}

bool runrel() {
    if (!release()){
        return false;
    }
    return run(EXECUTABLE_RELEASE);
}

// === main cli functions

typedef bool (*CommandFunc)();
struct Command {
    std::string name;
    std::string desc;
    CommandFunc func;
};

// valid commands
bool help();
std::vector<Command> COMMANDS = {
    { "check", "Check if required programs are installed", check_verbose },
    { "download", "Clone required libraries from Github or merge new commits.", download },
    { "gen", "Generate CMake project files", generate },
    { "generate", "Generate CMake project files (alias)", generate },
    { "build", "Build project (for debugging)", build },
    { "release", "Build optimized executable", release },
    { "package", "Build and package optimized executable", package },
    { "run", "Run executable (debug)", rundebug },
    { "runrel", "Run executable (release)", runrel },
    { "clean", "Remove build folder", clean },
    { "wipe", "clean + remove downloaded libraries", wipe },
    { "help", "Show this screen", help },
};

bool help(){
    Table table;
    for (const auto& cmd : COMMANDS){
        table.insert({cmd.name,cmd.desc});
    }
    print_table(table);
    return true;
}

bool run_option(std::string& option){
    if (option.empty()){
        std::cout << "No command given! " << std::endl;
        return false;
    }

    for (const auto& cmd : COMMANDS){
        if (cmd.name == option) {
            bool succeeded = (*cmd.func)();
            if (succeeded) {
                std::cout << "Done..." << std::endl;
                return true;
            }
            std::cerr << "Failed." << std::endl;
            return false;
        }
    }

    // invalid option
    std::cout << "Invalid option: " << option << std::endl;
    help();
    return false;
}

bool relocate(){
    try {
        auto path = fs::current_path();
        auto parentDir = path.parent_path();
        fs::current_path(parentDir);
        return true;
    } catch(std::exception const& ex){
        std::cerr << "Failed to relocate working directory!" << std::endl;
        return false;
    }
}

bool find_cmake_project(){
    if (!fs::exists("CMakeLists.txt")){
        //std::cout << "Execution directory does not contain CMakeLists.txt!" << std::endl;
        //std::cout << "Looking in parent directory..." << std::endl;
        if (relocate()){
            if (fs::exists("CMakeLists.txt")){
                //std::cout << "Found CMakeLists!" << std::endl;
                return true;
            }else{
                std::cout << "Didn't find CMakeLists.txt!" << std::endl;
            }
        }
        return false;
    }
    //std::cout << "Found CMakeLists!" << std::endl;
    return true;
}

int main(int argc, char** argv) {
    std::string option = argc > 1 ? std::string(argv[1]):"";
    if (option == "help"){
        help();
        return EXIT_SUCCESS;
    }

    // check if cmake file found
    if (!find_cmake_project() || !check()){
        return EXIT_FAILURE;
    }

    if (run_option(option)){
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

// UTILITY FUNCTIONS IMPLEMENTATION
static bool subfiles_exist(std::string path) {
    int count = 0;
    for (const auto& file : fs::directory_iterator(path)){
        if (!file.is_directory()){
            count++;
        }
    }
    return count > 0;
}

static std::string concat_sep(char sep, std::initializer_list<std::string> list) {
    std::string result = "";
    for (const auto elem : list) {
        result += elem;
        result += sep;
    }
    return result;
}

static bool make_dirs(std::string path){
    try {
        fs::create_directories(path);
        return true;
    } catch(fs::filesystem_error const& ex){
        std::cerr << "Failed to create directories!" << std::endl;
        std::cerr << ex.what() << std::endl;
        return false;
    }
}

static bool run_command(std::initializer_list<std::string> list){
    if (!system(NULL)){
        std::cerr << "No command processor found!" << std::endl;
        return false;
    }

    std::string cmd = concat_sep(' ',list);
    std::cout << ">> " << cmd << std::endl;

    int code = system(cmd.c_str());
    return code == 0;
}

static bool delete_recursive(std::string path){
    try {
        fs::remove_all(path);
        return true;
    } catch (fs::filesystem_error const& ex){
        std::cerr << "Failed to remove directories!" << std::endl;
        std::cerr << "TODO: You'll need to manually remove the folder for now." << std::endl;
        std::cerr << ex.what() << std::endl;
        return false;
    }
}

static void print_table(Table table, int padding){
    // calculate longest width first
    int width = 0;
    for (const auto& cmd : table){
        int len = cmd.first.length();
        if (len > width){
            width = len;
        }
    }
    width += padding;

    // print the table
    std::cout << std::endl;
    for (const auto& cmd : table){

        // pad string
        std::stringstream displayName;
        displayName << cmd.first;

        int spaces = width - cmd.first.length();
        for (int i = 0; i < spaces; i++){
            displayName << " ";
        }

        std::cout << displayName.str() << cmd.second << std::endl;
    }
}

static bool has_program(std::string program){
    const char* pathVar = std::getenv("PATH");
    if (pathVar == NULL){
        std::cerr << "Could not determine PATH variable" << std::endl;
        return false;
    }

#ifdef LINUX
    char seperator = ':';
#elif defined(WINDOWS)
    char seperator = ';';
#endif

    // TODO: there is probably a faster way to do this
    std::string path;
    std::istringstream stream(pathVar);
    while (std::getline(stream, path, ';')) {
        auto folder = fs::path(path);
        if (fs::exists(folder)) {
            for (const auto& file : fs::directory_iterator(folder)) {
                std::string stem = file.path().stem().string();
                if (stem == program) {
                    return true;
                }
            }
        }
    }
    return false;
}
