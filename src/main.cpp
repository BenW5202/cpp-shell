#include <iostream>
#include <string>

// REMINDER: codecrafters submit to test code

//if codecrafters keyword doesnt work

/*
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
*/


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  
  while (true){ 
    std::cout << "$ ";
    std::string input;
    std::cin >> input;

    if (input == "exit") {
      break;
    } else {
      std::cout << input << ": command not found" << std::endl;
    }
    
  }

}
