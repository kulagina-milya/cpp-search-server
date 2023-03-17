#pragma once

#include "search_server.h"
#include <vector>
#include <string>
#include <list>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer &s, const vector<string> &queries);

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries);