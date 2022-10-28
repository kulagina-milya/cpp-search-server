#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result;
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
	Document() = default;

	Document(int id, double relevance, int rating)
		: id(id)
		, relevance(relevance)
		, rating(rating) {
	}

	int id = 0;
	double relevance = 0.0;
	int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
	set<string> non_empty_strings;
	for (const string& str : strings) {
		if (!none_of(str.begin(), str.end(), [](char c) {
			return c >= '\0' && c < ' ';
		})) {
			throw invalid_argument("invalid stop word");
		}

		if (!str.empty()) {
			non_empty_strings.insert(str);
		}
	}
	return non_empty_strings;
}

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words)
		: stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
	}

	explicit SearchServer(const string& stop_words_text)
		: SearchServer(
			SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
	{
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
		if ((document_id < 0) || (documents_.count(document_id) > 0)) {
			throw invalid_argument("invalid id");
		}
		vector<string> words;
		if (!SplitIntoWordsNoStop(document, words)) {
			throw invalid_argument("invalid content document");
		}
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words) {
			word_to_document_freqs_[word][document_id] += inv_word_count;
		}
		documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
		documents_ids_.push_back(document_id);
	}

	template <typename DocumentPredicate>
	vector<Document> FindTopDocuments(const string& raw_query,
		DocumentPredicate document_predicate) const {
		Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, document_predicate);

		sort(matched_documents.begin(), matched_documents.end(),
			[](const Document& lhs, const Document& rhs) {
			if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
				return lhs.rating > rhs.rating;
			}
			else {
				return lhs.relevance > rhs.relevance;
			}
		});
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
		return matched_documents;
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
		return FindTopDocuments(
			raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
			return document_status == status;
		});
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {
		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}

	int GetDocumentCount() const {
		return static_cast<int>(documents_.size());
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
		int document_id) const {
		Query query = ParseQuery(raw_query);
		vector<string> matched_words;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.push_back(word);
			}
		}
		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			if (word_to_document_freqs_.at(word).count(document_id)) {
				matched_words.clear();
				break;
			}
		}
		return { matched_words, documents_.at(document_id).status };
	}

	int GetDocumentId(int index) const {
		if (index < 0 || index >= GetDocumentCount()) {
			throw out_of_range("index is not in range");
		}
		return documents_ids_[index];
	}

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};
	const set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;
	vector<int> documents_ids_;

	static bool IsValidWord(const string& word) {
		// A valid word must not contain special characters
		return none_of(word.begin(), word.end(), [](char c) {
			return c >= '\0' && c < ' ';
		});
	}

	bool IsStopWord(const string& word) const {
		return stop_words_.count(word) > 0;
	}

	bool SplitIntoWordsNoStop(const string& text, vector<string>& words) const {
		for (const string& word : SplitIntoWords(text)) {
			if (!IsValidWord(word)) {
				return false;
				break;
			}
			if (!IsStopWord(word)) {
				words.push_back(word);
			}
		}
		return true;
	}

	static int ComputeAverageRating(const vector<int>& ratings) {
		if (ratings.empty()) {
			return 0;
		}
		int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
		return rating_sum / static_cast<int>(ratings.size());
	}

	struct QueryWord {
		string data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(string text) const {
		if (!IsValidWord(text)) {
			throw invalid_argument("invalid symbols in word");
		}
		if (text[text.size() - 1] == '-') {
			throw invalid_argument("invalid symbol - in word");
		}
		bool is_minus = false;
		// Word shouldn't be empty
		if (text[0] == '-') {
			if ((text[1] == '-') || (text[1] == ' ')) {
				throw invalid_argument("invalid symbols in word");
			}
			is_minus = true;
			text = text.substr(1);
		};
		return { text, is_minus, IsStopWord(text) };
	}

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			QueryWord query_word = ParseQueryWord(word);
			if (!query_word.is_stop) {
				if (query_word.is_minus) {
					query.minus_words.insert(query_word.data);
				}
				else {
					query.plus_words.insert(query_word.data);
				}
			}
		}
		return query;
	}

	// Existence required
	double ComputeWordInverseDocumentFreq(const string& word) const {
		return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template <typename DocumentPredicate>
	vector<Document> FindAllDocuments(const Query& query,
		DocumentPredicate document_predicate) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto[document_id, term_freq] : word_to_document_freqs_.at(word)) {
				const auto& document_data = documents_.at(document_id);
				if (document_predicate(document_id, document_data.status, document_data.rating)) {
					document_to_relevance[document_id] += term_freq * inverse_document_freq;
				}
			}
		}

		for (const string& word : query.minus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto[document_id, _] : word_to_document_freqs_.at(word)) {
				document_to_relevance.erase(document_id);
			}
		}

		vector<Document> matched_documents;
		for (const auto[document_id, relevance] : document_to_relevance) {
			matched_documents.push_back(
				{ document_id, relevance, documents_.at(document_id).rating });
		}
		return matched_documents;
	}
};

// ==================== дл€ примера =========================
//закоментировала эту часть, так как сейчас выбрасываютс€ исключени€
/*
void PrintDocument(const Document& document) {
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating << " }"s << endl;
}
int main() {
	setlocale(LC_ALL, "Russian");
	SearchServer search_server("и в на"s);
	// явно игнорируем результат метода AddDocument, чтобы избежать предупреждени€
	// о неиспользуемом результате его вызова
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(1, "пушистый пЄс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
	search_server.AddDocument(-1, "пушистый пЄс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
	search_server.AddDocument(3, "большой пЄс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
	const auto documents = search_server.FindTopDocuments("--пушистый"s);
	for (const Document& document : documents) {
			PrintDocument(document);
	}

}*/
