#include "remove_duplicates.h"

void AddDocument(SearchServer& searchServer, int doc_id, const std::string& document, DocumentStatus status, const vector<int>& ratings) {
    searchServer.AddDocument(doc_id, document, status, ratings);
}

void RemoveDuplicates(SearchServer& search_server) {
    vector<int> ids;
    for (const int document_id : search_server) {
        map<string, double> words_freqs = search_server.GetWordFrequencies(document_id);
        set<string> words;
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
