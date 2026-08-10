#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

constexpr char PATH_LIST_SEPARATOR = ':';

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
  std::error_code ec;
  std::filesystem::file_status s = std::filesystem::status(p, ec);

  if (ec) {return false;}

  std::filesystem::perms perm = s.permissions();

  bool canExecute = ((perm & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ||
                    ((perm & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ||
                    ((perm & std::filesystem::perms::others_exec) != std::filesystem::perms::none);
  return canExecute;
}

std::string findExecutablePath(const std::string& command, const std::vector<std::string>& directories) {
  std::error_code ec;
  for (const std::string& d : directories) {
    std::filesystem::path potentialPath(d);
    potentialPath /= command;
    if (std::filesystem::is_regular_file(potentialPath, ec) && hasExecutePermission(potentialPath)) {
      return potentialPath.string();
    }
  }
  return "";
}

std::string findExecutablePath(const std::string& command) {
  return findExecutablePath(command, splitPath(getPathEnv()));
}

bool getData(std::string& command, std::vector<std::string>& args) {
  command = "";
  args.clear();

  std::cout << "$ ";
  std::string input;

  if (!std::getline(std::cin, input)) {
  return false;
  }

  std::vector<std::string> tokens;
  std::string currentToken;
  bool inSingle = false;
  bool inDouble = false;
  bool isEscaped = false;
  bool tokenStarted = false;


  for (char c : input) {
      if (isEscaped) {
          tokenStarted = true;
          currentToken.push_back(c);
          isEscaped = false;
      } else if (c == '\\' and !(inSingle)){
          tokenStarted = true;
          isEscaped = true;
      } else if (c == '\'' and !(inDouble)) {
          tokenStarted = true;
          inSingle = !(inSingle);
      } else if (c == '"' and !(inSingle)){
          tokenStarted = true;
          inDouble = !(inDouble);
      } else if (c == ' '){
          if (inSingle or inDouble){
            currentToken.push_back(c);
          } else {
            if(tokenStarted) {
              tokens.push_back(currentToken);
              currentToken = "";
              tokenStarted = false;
            } 
          }
      } else {
          tokenStarted = true;
          currentToken.push_back(c);
      }
  }
  if (tokenStarted){
      tokens.push_back(currentToken);
  }
  if (!(tokens.empty())){
      command = tokens[0];
      args.assign(tokens.begin() + 1, tokens.end());
  }
  return true;
}

bool isBuiltIn(std::string s) {
  inline static const std::unordered_set<std::string> builtInCommands = {"echo", "exit", "type"};

  if (builtInCommands.contains(s)) {return true;}
  
  return false;
}

void handleType(const std::vector<std::string>& args){
  if (!args.empty()) {
    if (isBuiltIn(args[0])) {
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
  std::cout << output << "\n";
}

bool handleExternalProgram(const std::string& path, const std::vector<std::string>& args){
  std::vector<const char*> vecOfPtrs;
  vecOfPtrs.push_back(path.c_str());

  for (const std::string& arg : args){
    vecOfPtrs.push_back(arg.c_str());
  }

  vecOfPtrs.push_back(nullptr);

  pid_t pid = fork();

  if (pid < 0) { //fork failed
    std::cerr << "ERROR: fork failed\n";
    return false;
  } else if (pid == 0) { //child process
    execv(path.c_str(), const_cast<char**>(vecOfPtrs.data()));
    std::cerr << "ERROR: child process failed\n";
    _exit(1);
  } else { //parent process
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) and WEXITSTATUS(status) == 0)  {
      return true;
    } else {
      return false;
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
    } else if (!(isBuiltIn(command))){
      if(std::string path = findExecutablePath(command); !path.empty()){
        //Execute the program
        if (!handleExternalProgram(path, args)){
          std::cerr << "ERROR: Failed to execute the program";
        }
      } else {
        std::cout << command << ": command not found" << "\n";
      }
    }
  }
}

  

 