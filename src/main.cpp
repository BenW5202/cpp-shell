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
#include <csignal>
#include <algorithm>

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

enum class Connector {
  NONE,
  PIPE, // |
  AND, // &&
  OR, // ||
  BACKGROUND // &
};

struct ParsedCommand {
  std::string command;
  std::vector<std::string> arguments;

  Connector connector = Connector::NONE;

  std::string inputFile = "";
  std::string outputFile = "";
  bool appendOutput = false; // true: '>>', false: '>'

};

bool getData(std::vector<ParsedCommand>& commands) {
  
  commands.clear();
  std::cout << "$ ";
  std::string input;

  if (!std::getline(std::cin, input)) {
    return false;
  }

  //Tokenization
  std::vector<std::string> tokens;
  std::string currentToken;
  bool inSingle = false;
  bool inDouble = false;
  bool isEscaped = false;
  bool tokenStarted = false;

  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
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
      } else if (!(inSingle) and !(inDouble)){
        if (c == '&' && i + 1 < input.length() &&
        input[i + 1] == '&'){
          if (tokenStarted) {
            tokens.push_back(currentToken);
            currentToken = "";
            tokenStarted = false;
          }
          tokens.push_back("&&");
          i++;
        } else if (c == '|' && i + 1 < input.length() &&
        input[i + 1] == '|'){
          if (tokenStarted) {
            tokens.push_back(currentToken);
            currentToken = "";
            tokenStarted = false;
          }
          tokens.push_back("||");
          i++;
        } else if (c == '>' && i + 1 < input.length() &&
        input[i + 1] == '>'){
          if (tokenStarted) {
            tokens.push_back(currentToken);
            currentToken = "";
            tokenStarted = false;
          }
          tokens.push_back(">>");
          i++;
        }
        //Single character operations
        else if (c == '&' || c == '|' ||
        c == '>' || c == '<') {
          if (tokenStarted) {
            tokens.push_back(currentToken);
            currentToken = "";
            tokenStarted = false;
          }
          tokens.push_back(std::string(1, c));
        }
        //Spaces
        else if (c == ' ' || c == '\t') {
          if (tokenStarted) {
            tokens.push_back(currentToken);
            currentToken = "";
            tokenStarted = false;
          }
        } else {
          tokenStarted = true;
          currentToken.push_back(c);
        }
      } else {
        tokenStarted = true;
        currentToken.push_back(c);
      }
    }

  if (tokenStarted) {
    tokens.push_back(currentToken);
  }
  
  //Parsing
  if (tokens.empty()) {return true;}

  ParsedCommand currentCmd;

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];

    if (token == "|") {
      currentCmd.connector = Connector::PIPE;
      commands.push_back(currentCmd);
      currentCmd = ParsedCommand();
    } else if (token == "&&") {
      currentCmd.connector = Connector::AND;
      commands.push_back(currentCmd);
      currentCmd = ParsedCommand();
    } else if (token == "||") {
      currentCmd.connector = Connector::OR;
      commands.push_back(currentCmd);
      currentCmd = ParsedCommand();
    } else if (token == "&") {
      currentCmd.connector = Connector::BACKGROUND;
      commands.push_back(currentCmd);
      currentCmd = ParsedCommand();
    } else if (token == ">") {
      if (i + 1 < tokens.size()) {
        currentCmd.outputFile = tokens[i + 1];
        currentCmd.appendOutput = false;
        i++;
      }
    } else if (token == ">>") {
      if (i + 1 < tokens.size()) {
        currentCmd.outputFile = tokens[i + 1];
        currentCmd.appendOutput = true;
        i++;
      }
    } else if (token == "<") {
      if (i + 1 < tokens.size()) {
        currentCmd.inputFile = tokens[i + 1];
        i++;
      }
    } else {
      if (currentCmd.command.empty()) {
        currentCmd.command = token;
      } else {
        currentCmd.arguments.push_back(token);
      }
    }
  } 

  if (!currentCmd.command.empty()) {
    commands.push_back(currentCmd);
  }
}

bool isBuiltIn(const std::string& s) {
  static const std::unordered_set<std::string> builtInCommands = {"echo", "exit", "type", "cd"};

  return std::find(builtInCommands.begin(), builtInCommands.end(), s) != builtInCommands.end();
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

void handleCd(const std::vector<std::string>& args){

  const char* path;

  if (args.empty() or args[0] == "~"){
    path = std::getenv("HOME");
  } else {
    path = args[0].c_str();
  }

  if (chdir(path) == -1){
    std::cerr << "ERROR: no such file or directory with path " << path << "\n";
  }
  
}

bool handleExternalProgram(const std::string& command, const std::string& path, const std::vector<std::string>& args){
  std::vector<const char*> vecOfPtrs;
  vecOfPtrs.push_back(command.c_str());

  for (const std::string& arg : args){
    vecOfPtrs.push_back(arg.c_str());
  }

  vecOfPtrs.push_back(nullptr);

  pid_t pid = fork();

  if (pid < 0) { //fork failed
    std::cerr << "ERROR: fork failed\n";
    return false;
  } else if (pid == 0) { //child process
    std::signal(SIGINT, SIG_DFL);
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

bool handlePipeline(const std::vector<ParsedCommand>& commands){
  
  int prev_fd = -1;

  for (size_t i = 0; i < commands.size(); ++i) {
    int pipefd[2];
    if (i != commands.size() - 1) {
      pipe(pipefd);
    }
    pid_t pid = fork();

    if (pid < 0) { 
      std::cerr << "ERROR: fork failed\n";
      return false;
    } else if (pid == 0) {
      if (prev_fd != -1) {
        dup2(prev_fd, STDIN_FILENO);
        close(prev_fd);
      }

      if (i != commands.size() - 1){
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
      }

      std::signal(SIGINT, SIG_DFL);

      
      std::string command = commands[i].command;
      std::vector<std::string> args = commands[i].arguments;

      if (isBuiltIn(command)) {
        if (command == "echo") {
          handleEcho(args);
        } else if (command == "type") {
          handleType(args);
        } else if (command == "cd") {
          handleCd(args);
        } else if (command == "exit") {
          _exit(0);
        }
        _exit(0);
      }

      std::string path = findExecutablePath(command);

      if (path.empty()) {
        std::cerr << command << ": command not found\n";
        _exit(127);
      }
      

      std::vector<const char*> vecOfPtrs;
      vecOfPtrs.push_back(command.c_str());

      for (const std::string& arg : args) {
        vecOfPtrs.push_back(arg.c_str());
      }

      vecOfPtrs.push_back(nullptr);

      execv(path.c_str(), const_cast<char**>(vecOfPtrs.data()));
      std::cerr << "ERROR: child process failed\n";
      _exit(1);

    } else {
      if (prev_fd != -1) {
        close(prev_fd);
      }
      if (i != commands.size() - 1){
        prev_fd = pipefd[0];
        close(pipefd[1]);
      }
    }    
  }
  while (waitpid(-1, NULL, 0) > 0) {

  }
  return true;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  //to prevent ctrl+c from terminating shell
  std::signal(SIGINT, SIG_IGN); 

  while (true){ 
    std::vector<ParsedCommand> commands;

    if (!getData(commands)){
      break;
    }
    if (commands.empty()) {
      continue;
    }
    if (commands.size() == 1){
      const ParsedCommand& cmd = commands[0];
      std::string command = cmd.command;
      std::vector<std::string> args = cmd.arguments;
      if (command == "exit") {
      break;
      } else if (command == "type") {
        handleType(args);
      } else if (command == "echo") {
        handleEcho(args);
      } else if (command == "cd") {
        handleCd(args);
      } else if (!(isBuiltIn(command))){
        if(std::string path = findExecutablePath(command); !path.empty()){
          //Execute the program
          if (!handleExternalProgram(command, path, args)){
            std::cerr << "ERROR: Failed to execute the program" << "\n";
          }
        } else {
          std::cout << command << ": command not found" << "\n";
        }
      }
    } else if (commands.size() > 1) {
      //pipe logic
      handlePipeline(commands);
    }
  }
}
  

 