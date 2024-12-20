#include "CheckUTF.h"
#include <string>
#include <iostream>
#include <fstream>

bool isValidUTF7Char(char c) {
    // Basic printable ASCII characters are valid in UTF-7
    if (c >= 32 && c <= 126) {
        return true;
    }

    // Other valid characters (excluding special characters used for encoding)
    if (c == '\t' || c == '\n' || c == '\r') {
        return true;
    }

    // UTF-7 uses '+' for encoding special characters
    if (c == '+') {
        return true;
    }

    return false;
}

bool isUTF7Encoded(const std::string& content)
{
    bool inEncoding = false; // Flag to track if we are inside a '+' encoded sequence
    for (char c : content) {
        if (inEncoding) {
            // Inside an encoded sequence, allowed characters are:
            // - Base64 characters (A-Za-z0-9+/) 
            // - Optional padding character (=)
            if (!isalnum(c) && c != '+' && c != '/' && c != '=') {
                return false; // Invalid character within encoded sequence
            }
            if (c == '-') {
                inEncoding = false; // End of encoded sequence
            }
        }
        else {
            if (c == '+') {
                inEncoding = true; // Start of encoded sequence
            }
            else if (!isValidUTF7Char(c)) {
                return false; // Invalid character outside encoded sequence
            }
        }
    }

    // If we are still in an encoded sequence at the end of the file, it's invalid
    if (inEncoding) {
        return false;
    }
    return true;
}
bool loadFileContent(const std::string& filename, std::string& input) {
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Could not open file " << filename << std::endl;
        std::cerr << "Starting proxy with empty blocklist..." << std::endl;

        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    file.close();
    
    if (isUTF7Encoded(content)) {
        input = content;
        return true;
    }
    return false;
}
