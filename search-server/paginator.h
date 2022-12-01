#pragma once

#include <iostream>
#include "document.h"
#include <sstream>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end) :
        begin_(begin), end_(end) {
    };

    Iterator begin() {
        return begin_;
    }
    Iterator end() {
        return end_;
    }
    size_t size() {
        return end_ - begin_;
    }
private:
    Iterator begin_;
    Iterator end_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, IteratorRange<Iterator> pages) {
    for (auto page = pages.begin(); page < pages.end(); page++) {
        out << *page;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    // тело класса
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        while (begin < end) {
            auto dist = distance(begin, end);
            if (dist >= page_size) {
                pages_.push_back(IteratorRange<Iterator>(begin, begin + page_size));
                advance(begin, page_size);
            }
            else {
                pages_.push_back(IteratorRange<Iterator>(begin, begin + dist));
                advance(begin, dist);
            }
        }
    };

    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }
private:
    vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}