#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) :
    searchServer_(search_server),
    time_(0) {
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    const auto docs = searchServer_.FindTopDocuments(raw_query, status);
    AddRequest(docs.empty());
    return docs;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    const auto docs = searchServer_.FindTopDocuments(raw_query);
    AddRequest(docs.empty());
    return docs;
}

int RequestQueue::GetNoResultRequests() const {
    int count = 0;
    for (auto req : requests_) {
        if (req.result == false) {
            ++count;
        }
    }
    return count;
}

void RequestQueue::AddRequest(const bool empty) {
    ++time_;
    if (time_ > min_in_day_) {
        requests_.pop_front();
    }
    QueryResult queryResult;
    queryResult.result = !empty;
    requests_.push_back(queryResult);
}
