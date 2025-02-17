#include <iostream>

// from
// https://mobiarch.wordpress.com/2023/12/17/reading-a-file-line-by-line-using-c-ranges/

struct FileLine {
  std::string line;
  size_t line_number = 0;

  friend std::istream &operator>>(std::istream &s, FileLine &fl) {
    std::getline(s, fl.line);

    ++fl.line_number;

    return s;
  }
};
