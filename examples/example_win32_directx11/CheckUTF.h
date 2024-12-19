#pragma once
#include <string>
#include <iostream>

bool isValidUTF7Char(char c);

bool isUTF7Encoded(const std::string& content);

bool loadFileContent(const std::string& filename, std::string& input);
