#include "search_server.h"
#include "string_processing.h"
#include <algorithm>
#include <math.h>

SearchServer::SearchServer(const std::string& stop_words_text) :
    SearchServer(std::string_view(stop_words_text)) {
}

SearchServer::SearchServer(std::string_view str) :
    SearchServer(SplitIntoWords(str)) {
}

//добавляет новый документ
void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    using namespace std;
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    all_docs_.emplace_back(document);
    const auto words = SplitIntoWordsNoStop(all_docs_.back());
    //const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const auto &word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy,
    std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy,
    std::string_view raw_query) const {
    return FindTopDocuments(raw_query);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy,
    std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy,
    std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}


int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    auto query = ParseQuery(raw_query);
    std::sort(query.minus_words.begin(), query.minus_words.end());
    const auto minus_word = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(minus_word, query.minus_words.end());
    std::sort(query.plus_words.begin(), query.plus_words.end());
    const auto plus_word = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(plus_word, query.plus_words.end());

    std::vector<std::string_view> matched_words;
    for (const auto& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return { matched_words, documents_.at(document_id).status };
        }
    }

    for (const auto& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy& policy,
    std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy& policy,
    std::string_view raw_query, int document_id) const {
    const auto& query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;
    if (std::any_of(policy, query.minus_words.begin(), query.minus_words.end(),
        [&](const auto& word) {
            return (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id));
        })) {
        matched_words.clear();
        return { matched_words, documents_.at(document_id).status };
    }
    matched_words.resize(query.plus_words.size());
    auto it = std::copy_if(policy, query.plus_words.begin(),
        query.plus_words.end(), matched_words.begin(),
        [&](auto word) {
            return (word_to_document_freqs_.count(word) && word_to_document_freqs_.at(word).count(document_id));
        }
    );
    matched_words.erase(it, matched_words.end());

    std::sort(matched_words.begin(), matched_words.end());
    const auto doc = std::unique(matched_words.begin(), matched_words.end());
    matched_words.erase(doc, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    using namespace std;
    std::vector<std::string_view> words;
   for (const auto &word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word.data() + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    using namespace std;
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + word.data() + " is invalid");
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query result;
    for (auto &word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::RemoveDocument(const int document_id) {
    const auto& words = GetWordFrequencies(document_id);
    for (auto &[word, freq] : words) {
        auto iter = word_to_document_freqs_.at(word).find(document_id);
        word_to_document_freqs_.at(word).erase(iter);
    }
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    {
        auto iter = find(document_ids_.begin(), document_ids_.end(), document_id);
        document_ids_.erase(iter);
    }
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& policy, const int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, const int document_id) {
   if (document_to_word_freqs_.count(document_id) == 0) {
        return;
    }
    const auto& words_freqs = GetWordFrequencies(document_id);
    vector<const std::string_view *> words(words_freqs.size());
    std::transform(policy,
        words_freqs.begin(), words_freqs.end(),
        words.begin(),
        [](const auto &word_freq) {
            return &word_freq.first;
        });

    std::for_each(policy,
        words.begin(), words.end(),
        [&](const auto& word) {
            word_to_document_freqs_.at(*word).erase(document_id);
        }
    );

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    {
        auto iter = find(document_ids_.begin(), document_ids_.end(), document_id);
        document_ids_.erase(iter);
    }
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    auto iter = document_to_word_freqs_.find(document_id);
    return iter->second;
}
/*
bool SearchServer::fillWordsIds(const set<string>& words, int id) {
    if (words_ids_.count(words) == 0) {
        words_ids_.insert({ words, id });
    }
    else {
        return false;
    }
    return true;
}

void AddDocument(SearchServer& searchServer, int doc_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    searchServer.AddDocument(doc_id, document, status, ratings);
}

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> ids;
    for (const int document_id : search_server) {
        std::map<std::string, double> words_freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string> words;
        for (auto [word, freq] : words_freqs) {
            words.insert(word);
        }
        if (!search_server.fillWordsIds(words, document_id)) {
            ids.push_back(document_id);
            std::cout << "Found duplicate document id " << document_id << std::endl;
        }
    }
    for (auto id : ids) {
        search_server.RemoveDocument(id);
    }
}
*/