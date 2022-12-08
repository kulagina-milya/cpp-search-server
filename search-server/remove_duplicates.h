#pragma once
#include "search_server.h"
#include "document.h"


//функция-обертка
void AddDocument(SearchServer& searchServer, int doc_id, const std::string& document, DocumentStatus status, const vector<int>& ratings);

void RemoveDuplicates(SearchServer& search_server);


