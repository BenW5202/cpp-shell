#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <filesystem>
#include <cstdlib>


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

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true){ 
    std::cout << "$ ";
    std::string input;
    
    if (!std::getline(std::cin, input)) {
      break;
    }

    std::stringstream ss(input);
    std::string command;
    ss >> command;

    if (command.empty()) {
      continue;
    }
    if (command == "exit") {
      break;
    } else if (command == "type") {
      std::string arg;
      if (ss >> arg) {
        if (builtInCommands.contains(arg)) {
          std::cout << arg << " is a shell builtin\n";
        } else if (std::string path = findExecutablePath(arg); !path.empty()) {
          std::cout << arg << " is " << path << "\n";
        } else {
          std::cout << arg << ": not found\n";
        }
      }
    } else if (command == "echo") {
      std::string args = "";
      if (input.length() > 5) {
        args = input.substr(5);
      }
      std::cout << args << "\n";
    } else {
      std::cout << input << ": command not found" << "\n";
    }
    
  }

}
