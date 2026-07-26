#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
// REMINDER: codecrafters submit to test code




int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  
  std::unordered_set<std::string> builtInCommands = {"echo", "exit", "type"};

  while (true){ 
    std::cout << "$ ";
    std::string input;
    
    if (!std::getline(std::cin, input)) {
      break;
    }

    std::stringstream ss(input);
    std::string command;
    ss >> command;

    if (command == "exit") {
      break;
    } else if (command == "type") {
      std::string arg;
      if (ss >> arg) {
        if (builtInCommands.find(arg) != builtInCommands.end()) {
          std::cout << arg << " is a shell builtin\n";
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
