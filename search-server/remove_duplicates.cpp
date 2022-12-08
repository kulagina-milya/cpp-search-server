#include "remove_duplicates.h"

void AddDocument(SearchServer& searchServer, int doc_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    searchServer.AddDocument(doc_id, document, status, ratings);
}

void RemoveDuplicates(SearchServer& search_server) {
    std::vector<int> ids;
    std::map<std::set<std::string>, int> document_to_id;
    for (const int document_id : search_server) {
        std::map<std::string, double> words_freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string> words;
        for (auto [word, freq] : words_freqs) {
            words.insert(word);
        }

        if (document_to_id.count(words) == 0) {
            document_to_id.insert({words, document_id});
        }
        else {
            ids.push_back(document_id);
            std::cout << "Found duplicate document id " << document_id << std::endl;
        }
    }
    for (auto id : ids) {
        search_server.RemoveDocument(id);
    }
}
