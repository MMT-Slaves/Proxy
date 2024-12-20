#pragma once
#ifndef DOMAINTRIE_H
#define DOMAINTRIE_H
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>

class Node {
public:
    Node* next[127] = { nullptr };
    bool end = 0;

    ~Node() {
        for (int i = 0; i < 127; i++) {
            delete next[i];
        }
    }
};

class DomainTrie {
public:
    Node* root;

    DomainTrie() {
        root = new Node();
    }

    DomainTrie(const std::vector<std::string> patterns) {
        root = new Node();
        for (const auto& pattern : patterns) {
            insert(pattern);
        }
    }

    ~DomainTrie() { delete root; }

    void insert(const std::string& pattern);

    bool search(const std::string& host);
};

#endif 
