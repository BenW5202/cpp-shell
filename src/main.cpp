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
#include <fcntl.h>
#include <cstring>

constexpr char PATH_LIST_SEPARATOR = ':';

std::vector<std::string> commandHistory;

void sigchld_handler(int signo) {
  (void)signo;

  while (waitpid(-1, nullptr, WNOHANG) > 0) {}
}

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

std::string getPrompt() {
  const char* user = std::getenv("USER");
  std::string userStr = user ?  user : "user";

  char host[256];
  std::string hostStr = "host";
  if (gethostname(host, sizeof(host)) == 0) {
    hostStr = host;
  }

  char cwd[1024];
  std::string cwdStr = "";
  if (getcwd(cwd, sizeof(cwd))) {
    cwdStr = cwd;
  }

  const char* home = std::getenv("HOME");
  if (home && cwdStr.find(home) == 0) {
    cwdStr.replace(0, std::strlen(home), "~");
  }

  return "\033[1;32m" + userStr + "@" + hostStr + "\033[0m:\033[1;34m" + cwdStr + "\033[0m$ ";
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
  std::cout << getPrompt();
  
  std::string input;

  if (!std::getline(std::cin, input)) {
    return false;
  }


  if (!input.empty()) {
    commandHistory.push_back(input);
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
  return true;
}

bool isBuiltIn(const std::string& s) {
  static const std::unordered_set<std::string> builtInCommands = {"echo", "exit", "type", "cd", "history"};

  return std::find(builtInCommands.begin(), builtInCommands.end(), s) != builtInCommands.end();
}

bool handleType(const std::vector<std::string>& args){
  if (!args.empty()) {return true;}
  if (isBuiltIn(args[0])) {
    std::cout << args[0] << " is a shell builtin\n";
    return true;
  }

  std::string path = findExecutablePath(args[0]);
  if (!path.empty()) {
    std::cout << args[0] << " is " << path << "\n";
    return true;
  }

  std::cout << args[0] << ": not found\n";
  return false;

}

bool handleEcho(const std::vector<std::string>& args){

  std::string output = "";

  for (size_t i = 0; i < args.size(); ++i){
    output += args[i];
    if (i < args.size() - 1){
      output += " ";
    }
  }
  std::cout << output << "\n";
  return true;
}

bool handleHistory() {
  for (size_t i = 0; i < commandHistory.size(); ++i) {
    std::cout << " " << (i + 1) << " " << commandHistory[i] << "\n";
  }
  return true;
}

bool handleCd(const std::vector<std::string>& args){

  const char* path;

  if (args.empty() or args[0] == "~"){
    path = std::getenv("HOME");
  } else {
    path = args[0].c_str();
  }

  if (chdir(path) == -1){
    std::cerr << "ERROR: no such file or directory with path " << path << "\n";
    return false;
  }
  return true;
}

bool applyRedirection(const ParsedCommand& cmd) {
  if (!cmd.inputFile.empty()) {
    int fd = open(cmd.inputFile.c_str(), O_RDONLY);
    if (fd < 0) {
      perror("Failed to open input file");
      return false;
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
  }
  if (!cmd.outputFile.empty()) {
    int flags = O_WRONLY | O_CREAT | (cmd.appendOutput ? O_APPEND : O_TRUNC);
    int fd = open(cmd.outputFile.c_str(), flags, 0644);
    if (fd < 0) {
      perror("Failed to open output file");
      return false;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }
  return true;
}

bool handleExternalProgram(const ParsedCommand& cmd, const std::string& path, bool runInBackground) {
  std::vector<const char*> vecOfPtrs;

  vecOfPtrs.push_back(cmd.command.c_str());
  for (const std::string& arg : cmd.arguments){
    vecOfPtrs.push_back(arg.c_str());
  }

  vecOfPtrs.push_back(nullptr);

  pid_t pid = fork();

  if (pid < 0) { //fork failed
    std::cerr << "ERROR: fork failed\n";
    return false;
  } else if (pid == 0) { //child process
    std::signal(SIGINT, SIG_DFL);
    if(!applyRedirection(cmd)) {_exit(1);}
    execv(path.c_str(), const_cast<char**>(vecOfPtrs.data()));
    std::cerr << "ERROR: child process failed\n";
    _exit(1);
  } else { //parent process
    
    if (runInBackground) {
      std::cout << "[1] " << pid << "\n";
      return true;
    }

    int status;
    waitpid(pid, &status, 0);

    return (WIFEXITED(status) and WEXITSTATUS(status) == 0);  
  }
}

bool handlePipeline(const std::vector<ParsedCommand>& commands, bool runInBackground){
  
  int prev_fd = -1;
  pid_t last_pid = -1;

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

      if (!applyRedirection(commands[i])) {_exit(1);}
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
        } else if (command == "history") {
          handleHistory();
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
      last_pid = pid;
      if (prev_fd != -1) {
        close(prev_fd);
      }
      if (i != commands.size() - 1){
        prev_fd = pipefd[0];
        close(pipefd[1]);
      }
    }    
  }

  if (runInBackground) {
    std::cout << "[1] " << last_pid << "\n";
    return true;
  }
  
  int status;
  int last_status = 0;
  pid_t wpid;
  while ((wpid = waitpid(-1, &status, 0)) > 0) {
    if (wpid == last_pid) {
      last_status = status;
    }
  }

  return (WIFEXITED(last_status) && WEXITSTATUS(last_status) == 0);
}

bool executeSingle(const ParsedCommand& cmd, bool runInBackground) {
  if (cmd.command == "exit") {
    exit(0);
  }

  if (isBuiltIn(cmd.command)) {
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    bool success = false;
    if (applyRedirection(cmd)) {
      if (cmd.command == "type") { 
        success = handleType(cmd.arguments);
      } else if (cmd.command == "echo") {
        success = handleEcho(cmd.arguments);
      } else if (cmd.command == "history") {
        success = handleHistory(); 
      }
      else if (cmd.command == "cd") {
        success = handleCd(cmd.arguments);
      }
    }

    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdin);
    close(saved_stdout);
    return success;
  }

  std::string path = findExecutablePath(cmd.command);
  if (!path.empty()) {
    return handleExternalProgram(cmd, path, runInBackground);
  } else {
    std::cout << cmd.command << ": command not found\n";
    return false; 
  }
}

bool executeBlock(const std::vector<ParsedCommand>& block, bool runInBackground) {
  if (block.empty()) {
    return true;
  }
  if (block.size() == 1) {
    return executeSingle(block[0], runInBackground);
  }

  return handlePipeline(block, runInBackground);
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  //to prevent ctrl+c from terminating shell
  std::signal(SIGINT, SIG_IGN); 

  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, nullptr);

  while (true){ 
    std::vector<ParsedCommand> commands;

    if (!getData(commands)){
      break;
    }
    if (commands.empty()) {
      continue;
    }
    size_t i = 0;
    bool last_status_success = true;

    while (i < commands.size()) {
      std::vector<ParsedCommand> block;
      
      while (i < commands.size()) {
        block.push_back(commands[i]);
        Connector c = commands[i].connector;
        i++;
        if (c != Connector::PIPE){
          break;
        }
      }

      Connector trailing_conn = block.back().connector;
      bool runInBackground = (trailing_conn == Connector::BACKGROUND);

      last_status_success = executeBlock(block, runInBackground);

      while (i < commands.size()) {
        if (trailing_conn == Connector::AND && !last_status_success) {

        } else if (trailing_conn == Connector::OR && last_status_success) {

        } else {
          break;
        }

        std::vector<ParsedCommand> skipped_block;
        while (i < commands.size()) {
          skipped_block.push_back(commands[i]);
          Connector c = commands[i].connector;
          i++;
          if (c != Connector::PIPE){
            break;
          }
        }
        trailing_conn = skipped_block.back().connector;
      }
    }
  }
  return 0;
}