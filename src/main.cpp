#include <iostream>
#include <string>

// REMINDER: codecrafters submit to test code

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::cout << "$ ";
  std::string input;
  std::cin >> input;

  std::cout << input << ": command not found";

}
