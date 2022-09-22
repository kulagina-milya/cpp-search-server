#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result = 0;
	cin >> result;
	ReadLine();
	return result;
}

vector<string> SplitIntoWords(const string& text) {
	vector<string> words;
	string word;
	for (const char c : text) {
		if (c == ' ') {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		}
		else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}

	return words;
}

struct Document {
	int id;
	double relevance;
};

struct Query {
	set<string> plusWords;
	set<string> minusWords;
};

class SearchServer {
public:
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document) {
		const vector<string> words = SplitIntoWordsNoStop(document);
		int countWords = words.size();
		for (const auto & str : words) {
			double k = 1.0*count(words.begin(), words.end(), str) / countWords;
			word_to_document_freqs_[str].insert({ document_id, k });
		}
		++document_count_;
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {
		const Query query_words = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query_words);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
			return lhs.relevance > rhs.relevance;
		});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

private:
	map<string, map<int, double>> word_to_document_freqs_;

	set<string> stop_words_;

	int document_count_ = 0;

	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return words;
	}

	Query ParseQuery(const string& text) const {
		Query query_words;
		for (string& word : SplitIntoWords(text)) {
			if (word[0] == '-') {
				word = word.substr(1, word.size());
				query_words.minusWords.insert(word);
			}
			else {
				if (!IsStopWord(word)) {
					query_words.plusWords.insert(word);
				}
			}
		}
		return query_words;
	}

	vector<Document> FindAllDocuments(const Query& query_words) const {
		vector<Document> matched_documents;
		map<int, double> id_to_relevance;
		int amountDocument = 0;
		for (const auto & word : query_words.plusWords) {
			if (word_to_document_freqs_.count(word) > 0) {
				map<int, double> temp = word_to_document_freqs_.at(word);
				amountDocument = temp.size();
				double idf = log(1.0* document_count_ / amountDocument);
				for (auto & id_tf : temp) {
					id_tf.second *= idf;
					id_to_relevance[id_tf.first] += id_tf.second;
				}
			}
		}

		for (const auto & word : query_words.minusWords) {
			if (word_to_document_freqs_.count(word) > 0) {
				map<int, double> temp = word_to_document_freqs_.at(word);
				for (auto & id_tf : temp) {
					id_to_relevance.erase(id_tf.first);
				}
			}
		}

		for (const auto & id_rel : id_to_relevance) {
			matched_documents.push_back({ id_rel.first, id_rel.second });
		}

		return matched_documents;
	}
};

SearchServer CreateSearchServer() {
	SearchServer search_server;
	search_server.SetStopWords(ReadLine());

	const int document_count = ReadLineWithNumber();
	for (int document_id = 0; document_id < document_count; ++document_id) {
		search_server.AddDocument(document_id, ReadLine());
	}
	return search_server;
}

int main() {
	const SearchServer search_server = CreateSearchServer();

	const string query = ReadLine();
	for (const auto&[document_id, relevance] : search_server.FindTopDocuments(query)) {
		cout << "{ document_id = "s << document_id << ", "
			<< "relevance = "s << relevance << " }"s << endl;
	}
}
