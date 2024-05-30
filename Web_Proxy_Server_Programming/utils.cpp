#include "utils.h"

/**
  @brief Change all characters in a string to lowercase

  @param[in]  str  Input string

  @return  std::string  Lowercase string
**/
std::string lower(const std::string &str) {
  std::string ret = str;
  for (auto &c : ret) {
    c = std::tolower(c);
  }
  return ret;
}

/**
  @brief Replace all instances of a character in a string with another character

  @param[in]  str  Input string
  @param[in]  c1   Character to replace
  @param[in]  c2   Character to replace with

  @return  std::string  String with characters replaced
**/
std::string replace(const std::string &str, char c1, char c2) {
  std::string ret = str;
  for (auto &c : ret) {
    if (c == c1) {
      c = c2;
    }
  }
  return ret;
}

/**
  @brief Remove leading and trailing instances of given characters from a string

  @param[in]  str    Input string
  @param[in]  chars  Characters to remove

  @return  std::string  String with leading and trailing characters removed
**/
std::string strip(const std::string &str, const std::string &chars) {
  size_t start = str.find_first_not_of(chars), end = str.find_last_not_of(chars);
  if (start == std::string::npos || start > end) {
    return std::string();
  }
  return str.substr(start, end - start + 1);
}

/**
  @brief Normalizes a field name by converting it to title case

  @param[in]  str  Input field name

  @return  Normalized field name
**/
std::string normalize_field_name(const std::string &str) {
  bool        next_caps = true;
  std::string ret       = str;
  for (auto &c : ret) {
    if (next_caps) {
      c         = std::toupper(c);
      next_caps = false;
    } else {
      c = std::tolower(c);
    }
    if (c == '-') {
      next_caps = true;
    }
  }
  return ret;
}

/**
  @brief Attempts to compute the terminal width using ioctl, returns 80 on failure


  @return Terminal width or 80 on failure
**/
int get_terminal_width() {
  struct winsize win_size;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_size) < 0) {
    return 80;
  }
  return win_size.ws_col;
}
