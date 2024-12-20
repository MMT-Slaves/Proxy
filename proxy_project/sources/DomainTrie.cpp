#include "DomainTrie.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>

void DomainTrie::insert(const std::string& domain) {
    Node* currNode = root;
    for (int i = domain.length() - 1; i >= 0; i--) {
        const char key = domain[i];
        if (!currNode->next[key]) {
            currNode->next[key] = new Node();
        }
        currNode = currNode->next[key];
    }
    currNode->end = true;
}

bool DomainTrie::search(const std::string& host) {
    int n = host.length();
    Node* cur = root;

    for (int i = n - 1; i >= 0; i--) {
        char ch = host[i];
        if (cur->next['*']) return true;

        if (cur->next[ch]) {
            cur = cur->next[ch];
        }
        else break;

        if (i == 0 && cur->end) return true;
    }
    return false;
}
