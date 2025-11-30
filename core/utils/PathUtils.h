#pragma once
#include <string>
#include <vector>
#include <sstream>

class PathUtils {
private:

    static constexpr char DELIMETERS[] = {'/'};

    static bool isDelimeter(const char& ch) {
        for (auto& delimeter: DELIMETERS) {
            if (ch == delimeter) {
                return true;
            }
        }

        return false;
    }

public:

    static std::vector<std::string> split(const std::string& path) {
        std::vector<std::string> parts;
        
        size_t left = 0;
        for (size_t right = 0; right < path.length(); right++) {
            if (isDelimeter(path[right])) {
                if (left != right) {
                    parts.push_back(path.substr(left, right - left));
                }
                left = right + 1;
            }
        }

        if (left < path.size()) {
            parts.push_back(path.substr(left));
        }

        return parts;
    }
};