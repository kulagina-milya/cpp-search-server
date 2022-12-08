#pragma once

#include "search_server.h"
#include "document.h"

//функция-обертка
void AddDocument(SearchServer& searchServer, int doc_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

void RemoveDuplicates(SearchServer& search_server);
