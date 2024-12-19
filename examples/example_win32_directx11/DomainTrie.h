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

class TrieNode {
public:
    std::unordered_map<std::string, std::shared_ptr<TrieNode>> children;
    bool is_exact_match = false;  // True if this node represents an exact match (e.g., "google.com")
    bool is_wildcard = false;     // True if this node represents a wildcard (e.g., "*.google.com")
};

class DomainTrie {
public:
    DomainTrie() : root(std::make_shared<TrieNode>()) {}

    // Insert a domain into the Trie
    void insert(const std::string& domain);

    // Search for an exact or wildcard match for a given host
    bool search(const std::string& host) const;

    void delTreeHelper(std::shared_ptr<TrieNode>& node);

    void delTree();

private:
    std::shared_ptr<TrieNode> root;

    bool search_helper(const std::vector<std::string>& segments, size_t index, std::shared_ptr<TrieNode> node, bool allowWildcard) const;

    // Split domain into segments in reverse order (e.g., "www.example.com" -> ["com", "example", "www"])
    static std::vector<std::string> split_domain(const std::string& domain);
};


#endif 
