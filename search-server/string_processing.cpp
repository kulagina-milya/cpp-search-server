#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view text) {
    std::vector<std::string_view> words;
    if (text.empty()) {
        return words;
    }
    size_t j = 0;
    for (size_t i = 0; i < text.size() - 1; ++i) {
        if (text[i] == ' ') {
            if (!text.substr(j, i - j).empty()) {
                words.push_back(text.substr(j, i - j));
                j = ++i;
            }
        }
    }
    if (!text.substr(j, text.length() - 1).empty()) {
        words.push_back(text.substr(j, text.length() - 1));
    }

    return words;
}