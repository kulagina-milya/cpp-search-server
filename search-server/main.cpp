#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <numeric>
#include <vector>
#include <iostream>

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
	int id;
	double relevance;
	int rating;
};

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED,
};

class SearchServer {
public:
	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status,
		const vector<int>& ratings) {
		const vector<string> words = SplitIntoWordsNoStop(document);
		const double inv_word_count = 1.0 / words.size();
		for (const string& word : words) {
			word_to_document_freqs_[word][document_id] += inv_word_count;
		}
		documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
	}

	vector<Document> FindTopDocuments(const string& raw_query) const {
		return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
		return FindTopDocuments(raw_query,
			[status](int document_id, DocumentStatus docStatus, int rating) {
			return docStatus == status;
		});
	}

	template <typename KeyMapper>
	vector<Document> FindTopDocuments(const string& raw_query, KeyMapper key_mapper) const {
		const Query query = ParseQuery(raw_query);
		auto matched_documents = FindAllDocuments(query, key_mapper);

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

	int GetDocumentCount() const {
		return documents_.size();
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
		int document_id) const {
		const Query query = ParseQuery(raw_query);
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

private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
	};

	set<string> stop_words_;
	map<string, map<int, double>> word_to_document_freqs_;
	map<int, DocumentData> documents_;

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
		bool is_minus = false;
		// Word shouldn't be empty
		if (text[0] == '-') {
			is_minus = true;
			text = text.substr(1);
		}
		return { text, is_minus, IsStopWord(text) };
	}

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& text) const {
		Query query;
		for (const string& word : SplitIntoWords(text)) {
			const QueryWord query_word = ParseQueryWord(word);
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

	template <typename KeyMapper>
	vector<Document> FindAllDocuments(const Query& query, KeyMapper key_mapper) const {
		map<int, double> document_to_relevance;
		for (const string& word : query.plus_words) {
			if (word_to_document_freqs_.count(word) == 0) {
				continue;
			}
			const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
			for (const auto[document_id, term_freq] : word_to_document_freqs_.at(word)) {
				if (key_mapper(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
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

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
	const string& func, unsigned line, const string& hint) {
	if (t != u) {
		cerr << boolalpha;
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cerr << t << " != "s << u << "."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
	const string& hint) {
	if (!value) {
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT("s << expr_str << ") failed."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TFUNC>
void RunTestImpl(TFUNC func, const string& f_name) {
	func();
	cerr << f_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// -------- Ќачало модульных тестов поисковой системы ----------

// “ест провер€ет, что поискова€ система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	// —начала убеждаемс€, что поиск слова, не вход€щего в список стоп-слов,
	// находит нужный документ
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	// «атем убеждаемс€, что поиск этого же слова, вход€щего в список стоп-слов,
	// возвращает пустой результат
	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT(server.FindTopDocuments("in"s).empty());
	}
}

//“ест провер€ет, что добавленный документ находитс€ по поисковому запросу, который содержит слова из документа
void TestSearchAddedDocument() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	const int another_doc_id = 43;
	const string another_content = "black dog with big ears"s;
	const vector<int> another_ratings = { 5, 12, 0 };
	// ѕроверка добавлени€ документа и поиска по запросу
	SearchServer server;
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	server.AddDocument(another_doc_id, another_content, DocumentStatus::ACTUAL, another_ratings);
	const auto found_docs = server.FindTopDocuments("white cat"s);
	ASSERT_EQUAL_HINT(found_docs.size(), 1, "Sizes are not equal");
	const Document& doc0 = found_docs[0];
	ASSERT_EQUAL_HINT(doc0.id, doc_id, "Wrong id");
}

//“ест провер€ет, что документы, содержащие минус-слова поискового запроса, не включаютс€ в результаты поиска
void TestDocumentWithMinusWords() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	const int another_doc_id = 43;
	const string another_content = "black cat with big ears"s;
	const vector<int> another_ratings = { 5, 12, 0 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(another_doc_id, another_content, DocumentStatus::ACTUAL, another_ratings);
		const auto found_docs = server.FindTopDocuments("cat -big"s);
		ASSERT_EQUAL(found_docs.size(), 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}
	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(another_doc_id, another_content, DocumentStatus::ACTUAL, another_ratings);
		ASSERT_HINT(server.FindTopDocuments("white black -cat"s).empty(), "Not empty");
	}
}

//ѕри матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе
// ≈сли есть соответствие хот€ бы по одному минус-слову, должен возвращатьс€ пустой список слов.
void TestMatchDocument() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	SearchServer server;
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	tuple<vector<string>, DocumentStatus> match_words = server.MatchDocument("cat"s, doc_id);
	ASSERT_EQUAL(get<0>(match_words)[0], "cat");
}

// “ест провер€ет, что возвращаемые при поиске документов результаты отсортированы в пор€дке убывани€ релевантности
void TestSortDocumentLessRelevance() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	const int second_doc_id = 43;
	const string second_content = "black cat with big ears"s;
	const vector<int> second_ratings = { 5, 12, 0 };
	const int third_doc_id = 54;
	const string third_content = "beautiful cat in brown box"s;
	const vector<int> third_ratings = { 8, 8, 8, 1 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(second_doc_id, second_content, DocumentStatus::ACTUAL, second_ratings);
		server.AddDocument(third_doc_id, third_content, DocumentStatus::ACTUAL, third_ratings);
		const auto found_docs = server.FindTopDocuments("white cat city ears"s);
		ASSERT_EQUAL(found_docs.size(), 3);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
		const Document& doc1 = found_docs[1];
		ASSERT_EQUAL(doc1.id, second_doc_id);
		const Document& doc2 = found_docs[2];
		ASSERT_EQUAL(doc2.id, third_doc_id);
	}
};

//“ест провер€ет, что рейтинг добавленного документа равен среднему арифметическому оценок документа
void TestDocumentRatings() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	const int second_doc_id = 43;
	const string second_content = "black cat with big ears"s;
	const vector<int> second_ratings = { 5, 5, 5 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(second_doc_id, second_content, DocumentStatus::ACTUAL, second_ratings);
		const auto found_docs = server.FindTopDocuments("white cat"s);
		ASSERT_EQUAL(found_docs.size(), 2);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
		ASSERT_EQUAL(doc0.rating, 2);
		const Document& doc1 = found_docs[1];
		ASSERT_EQUAL(doc1.id, second_doc_id);
		ASSERT_EQUAL(doc1.rating, 5);
	}
}

//“ест провер€ет фильтрацию результатов поиска с использованием предиката, задаваемого пользователем
void TestPredicat() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	const int second_doc_id = 43;
	const string second_content = "black cat with big ears"s;
	const vector<int> second_ratings = { 5, 12, 0 };
	const int third_doc_id = 54;
	const string third_content = "beautiful cat in brown box"s;
	const vector<int> third_ratings = { 8, 8, 8, 1 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(second_doc_id, second_content, DocumentStatus::BANNED, second_ratings);
		server.AddDocument(third_doc_id, third_content, DocumentStatus::ACTUAL, third_ratings);
		const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) {return document_id % 2 == 0; });
		ASSERT_EQUAL(found_docs.size(), 2);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, third_doc_id);
		const Document& doc1 = found_docs[1];
		ASSERT_EQUAL(doc1.id, doc_id);
	}
}

//“ест провер€ет поиск документов, имеющих заданный статус
void TestSearchDocumentAndStatus() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	const int another_doc_id = 43;
	const string another_content = "black cat with big ears"s;
	const vector<int> another_ratings = { 5, 12, 0 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		server.AddDocument(another_doc_id, another_content, DocumentStatus::BANNED, another_ratings);
		const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
		ASSERT_EQUAL(found_docs.size(), 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, another_doc_id);
	}
}

// орректное вычисление релевантности найденных документов
void TestCorrectCalculateRelevance() {
	const int doc_id = 42;
	const string content = "white cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("white cat city"s);
		ASSERT_EQUAL(found_docs.size(), 1);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
		ASSERT(doc0.relevance - 0.277259 < EPSILON);
	}
}

// ‘ункци€ TestSearchServer €вл€етс€ точкой входа дл€ запуска тестов
void TestSearchServer() {
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestSearchAddedDocument);
	RUN_TEST(TestDocumentWithMinusWords);
	RUN_TEST(TestMatchDocument);
	RUN_TEST(TestSortDocumentLessRelevance);
	RUN_TEST(TestDocumentRatings);
	RUN_TEST(TestPredicat);
	RUN_TEST(TestSearchDocumentAndStatus);
	RUN_TEST(TestCorrectCalculateRelevance);
}

// --------- ќкончание модульных тестов поисковой системы -----------

int main() {
	TestSearchServer();
	// ≈сли вы видите эту строку, значит все тесты прошли успешно
	cout << "Search server testing finished"s << endl;
}