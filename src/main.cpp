#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>


#ifdef _WIN32
  constexpr char PATH_LIST_SEPARATOR = ';';
#else
  constexpr char PATH_LIST_SEPARATOR = ':';
#endif

std::unordered_set<std::string> builtInCommands = {"echo", "exit", "type"};

std::string getPathEnv() {
  const char* pathVal = std::getenv("PATH");
  return pathVal ? pathVal : "";
}

std::vector<std::string> splitPath(const std::string& pathVal) {
  std::vector<std::string> directories = {};
  std::stringstream ss(pathVal);
  std::string token;
  while (std::getline(ss, token, PATH_LIST_SEPARATOR)) {
    if (token.empty()) {
      directories.push_back(".");
    } else {
      directories.push_back(token);
    }
  }
  return directories;
}

bool hasExecutePermission(const std::filesystem::path& p){
  std::filesystem::perms perm = std::filesystem::status(p).permissions();

  bool canExecute = ((perm & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ||
                    ((perm & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ||
                    ((perm & std::filesystem::perms::others_exec) != std::filesystem::perms::none);
  return canExecute;
}

std::string findExecutablePath(const std::string& command, const std::vector<std::string>& directories) {
  for (const std::string& d : directories) {
    std::filesystem::path potentialPath(d);
    potentialPath /= command;
    if (std::filesystem::is_regular_file(potentialPath) && hasExecutePermission(potentialPath)) {
      return potentialPath.string();
    }
  }
  return "";
}

std::string findExecutablePath(const std::string& command) {
  return findExecutablePath(command, splitPath(getPathEnv()));
}

bool getData(std::string& command, std::vector<std::string>& args) {
  std::cout << "$ ";
  std::string input;
    
  if (!std::getline(std::cin, input)) {
    return false;
  }

  std::stringstream ss(input);
  ss >> command;

  std::string temp;
  args.clear();
  while (ss >> temp) {
    args.push_back(temp);
  }

  return true;
}

void handleType(const std::vector<std::string>& args){
  if (!args.empty()) {
    if (builtInCommands.contains(args[0])) {
      std::cout << args[0] << " is a shell builtin\n";
    } else if (std::string path = findExecutablePath(args[0]); !path.empty()) {
      std::cout << args[0] << " is " << path << "\n";
    } else {
      std::cout << args[0] << ": not found\n";
    }
  }
}

void handleEcho(const std::vector<std::string>& args){

  std::string output = "";

  for (size_t i = 0; i < args.size(); ++i){
    output += args[i];
    if (i < args.size() - 1){
      output += " ";
    }
  }
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true){ 
    std::string command;
    std::vector<std::string> args;

    if (!getData(command, args)){
      break;
    }
    if (command.empty()) {
      continue;
    }
    if (command == "exit") {
      break;
    } else if (command == "type") {
      handleType(args);
    } else if (command == "echo") {
      handleEcho(args);
    } else if (!(builtInCommands.contains(command))){
      if(std::string path = findExecutablePath(command); !path.empty()){
        //Execute the program
        break;
        }
      } else {
        std::cout << command << ": command not found" << "\n";
      }
  }
}

  

