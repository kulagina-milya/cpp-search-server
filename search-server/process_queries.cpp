#include "process_queries.h"
#include <execution>
#include <algorithm>

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> documents_lists(queries.size());
	transform(execution::par,
		queries.begin(), queries.end(),
		documents_lists.begin(),
		[&search_server, &queries](const std::string query) {
			return search_server.FindTopDocuments(query);
		}
	);
	return documents_lists;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> documents_lists = ProcessQueries(search_server, queries);
	std::list<Document> docs;
	for (const auto& documents : documents_lists) {
		for (const auto& doc : documents) {
			docs.push_back(doc);
		}
	}
	return docs;
}