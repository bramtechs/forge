// Based on Tsoding Daily: New Build System in C — Part 1
// https://youtu.be/n47AFxc1ksE

#include <iostream>
#include <vector>

#ifdef __linux__
    #define LINUX
#elif defined(_WIN32) || defined(WIN32)
    #define WINDOWS
#endif

// =============================
// === PROJECT CONFIGURATION ===
// =============================

// set to true if configured properly
constexpr bool IS_CONFIGURED = true;

// executable file
#ifdef LINUX
std::string EXECUTABLE_DEBUG = "build/Debug/forge_example.exe";
std::string EXECUTABLE_RELEASE = "build/Release/forge_example.exe";
#elif defined(WINDOWS)
// TODO:
std::string EXECUTABLE_DEBUG = "build/Debug/forge_example.exe";
std::string EXECUTABLE_RELEASE = "build/Release/forge_example.exe";
#endif

// UNIMPLEMENTED: tools required to build the program
std::vector<std::string> REQUIRED_TOOLS = {
    "cmake",
    "git",
#ifdef LINUX
    "g++",
#elif defined(WINDOWS)
    "cl",
#endif
};

// online libraries required to build the program
std::vector<std::pair<std::string,std::string>> REQUIRED_REPOS = {
    { "libs/raylib","https://github.com/bramtechs/raylib-lite.git" },
};

// =============================

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <initializer_list>
#include <filesystem>
#include <unordered_map>
#include <sstream>
#include <thread>

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

#define PATH_SEP std::filesystem::path::preferred_separator

bool subfiles_exist(std::string path) {
    int count = 0;
    for (const auto& file : std::filesystem::directory_iterator(path)){
        if (!file.is_directory()){
            count++;
        }
    }
    return count > 0;
}

std::string concat_sep(char sep, std::initializer_list<std::string> list) {
    std::string result = "";
    for (const auto elem : list) {
        result += elem;
        result += sep;
    }
    return result;
}

bool make_dirs(std::initializer_list<std::string> list){
    std::string path = concat_sep(PATH_SEP,list);
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch(std::filesystem::filesystem_error const& ex){
        std::cerr << "Failed to create directories!" << std::endl;
        std::cerr << ex.what() << std::endl;
        return false;
    }
}

bool run_command(std::initializer_list<std::string> list){
    if (!system(NULL)){
        std::cerr << "No command processor found!" << std::endl;
        return false;
    }

    std::string cmd = concat_sep(' ',list);
    std::cout << ">> " << cmd << std::endl;

    int code = system(cmd.c_str());
    return code == 0;
}

bool delete_recursive(std::string path){
    try {
        std::filesystem::remove_all(path);
        return true;
    } catch (std::filesystem::filesystem_error const& ex){
        std::cerr << "Failed to remove directories!" << std::endl;
        std::cerr << "TODO: You'll need to manually remove the folder for now." << std::endl;
        std::cerr << ex.what() << std::endl;
        return false;
    }
}

typedef std::unordered_map<std::string,std::string> Table;

void print_table(Table table, int padding=3){
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

bool has_program(std::string program){
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
        auto folder = std::filesystem::path(path);
        if (std::filesystem::exists(folder)) {
            for (const auto& file : std::filesystem::directory_iterator(folder)) {
                std::string stem = file.path().stem().string();
                if (stem == program) {
                    return true;
                }
            }
        }
    }
    return false;
}

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
    if (std::filesystem::exists("build")){
        return delete_recursive("build");
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
    delete_recursive("build");
    return true;
}

bool download() {
    // download libraries
    std::cout << "Checking if libraries are present..." << std::endl;
    for (const auto& repo : REQUIRED_REPOS) {
        if (std::filesystem::exists(repo.first) && subfiles_exist(repo.first)) {
            std::cout << "Updating repo " << repo.first << std::endl;

            if (!run_command({"git", "fetch", repo.first, "-p"})){
                return false;
            }
            if (!run_command({"cd",repo.first,"&","git", "merge"})){
                return false;
            }

        } else {

            // fail-safe if folder sticked around
            if (std::filesystem::exists(repo.first)){ 
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

bool generate() {
    if (!download()){
        return false;
    }

    std::cout << "Generating cmake project..." << std::endl;
#ifdef LINUX
    return run_command({"cmake", "-S", ".", "-B", "build", "-G", "\"CodeBlocks - Unix Makefiles\"" });
#elif defined(WINDOWS)
    return run_command({"cmake", "-S", ".", "-B", "build", "-G", "\"Visual Studio 17 2022\"", "-A", "Win32"});
#endif
}

bool build() {
    if (!std::filesystem::exists("build")){
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
    if (!std::filesystem::exists("build")){
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

bool package(){
    if (!release()){
        return false;
    }

    return false;
}

bool run(std::string& path) {
    auto p = std::filesystem::path(path.c_str());
    std::string file = p.filename().string();
    std::string folder = p.parent_path().string();

    if (std::filesystem::exists(path)) {
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
        auto path = std::filesystem::current_path();
        auto parentDir = path.parent_path();
        std::filesystem::current_path(parentDir);
        return true;
    } catch(std::exception const& ex){
        std::cerr << "Failed to relocate working directory!" << std::endl;
        return false;
    }
}

bool find_cmake_project(){
    if (!std::filesystem::exists("CMakeLists.txt")){
        //std::cout << "Execution directory does not contain CMakeLists.txt!" << std::endl;
        //std::cout << "Looking in parent directory..." << std::endl;
        if (relocate()){
            if (std::filesystem::exists("CMakeLists.txt")){
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

    if (IS_CONFIGURED){
        if (run_option(option)){
            return EXIT_SUCCESS;
        }
    }else{
        std::cout << "Edit forge.cpp first and configure the program!" << std::endl;
    }

    return EXIT_FAILURE;
}
