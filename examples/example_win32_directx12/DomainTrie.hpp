#pragma once

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
    void insert(const std::string& domain) {
        auto node = root;
        auto segments = split_domain(domain);
        bool is_wildcard = segments[0] == "*";

        // Insert each segment into the Trie
        for (const auto& segment : segments) {
            if (!node->children[segment]) {
                node->children[segment] = std::make_shared<TrieNode>();
            }
            node = node->children[segment];
        }

        // Mark the end of domain: exact match or wildcard
        node->is_exact_match = !is_wildcard;
        node->is_wildcard = is_wildcard;
    }

    // Search for an exact or wildcard match for a given host
    bool search(const std::string& host) const {
        auto segments = split_domain(host);
        return search_helper(segments, 0, root, /*allowWildcard=*/true);
    }

private:
    std::shared_ptr<TrieNode> root;

    bool search_helper(const std::vector<std::string>& segments, size_t index, std::shared_ptr<TrieNode> node, bool allowWildcard) const {
        if (!node) return false;

        // If we've reached the end of the host segments
        if (index == segments.size()) {
            return node->is_exact_match;
        }

        const auto& segment = segments[index];

        // Exact match for the current segment
        if (node->children.find(segment) != node->children.end()) {
            if (search_helper(segments, index + 1, node->children.at(segment), allowWildcard)) {
                return true;
            }
        }

        // Check for wildcard only if allowed (skip wildcard for exact matches)
        if (allowWildcard && node->children.find("*") != node->children.end()) {
            return search_helper(segments, index + 1, node->children.at("*"), /*allowWildcard=*/false);
        }

        return false;
    }

    // Split domain into segments in reverse order (e.g., "www.example.com" -> ["com", "example", "www"])
    static std::vector<std::string> split_domain(const std::string& domain) {
        std::vector<std::string> segments;
        std::stringstream ss(domain);
        std::string segment;

        while (std::getline(ss, segment, '.')) {
            segments.push_back(segment);
        }

        std::reverse(segments.begin(), segments.end());
        return segments;
    }
};
