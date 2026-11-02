#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "quote_manager.h"

// ==========================================
// 1. HELPER FUNCTIONS
// ==========================================

// Case-insensitive string comparison helper
int case_insensitive_compare(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Case-insensitive substring search helper
bool contains_exact_substring(const char *str, const char *keyword) {
    if (!str || !keyword) return false;
    int len_str = strlen(str);
    int len_kw = strlen(keyword);
    if (len_kw == 0) return true;

    for (int i = 0; i <= len_str - len_kw; i++) {
        bool match = true;
        for (int j = 0; j < len_kw; j++) {
            if (tolower((unsigned char)str[i + j]) != tolower((unsigned char)keyword[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// Minimum of three integers helper
int min_of_three(int a, int b, int c) {
    int m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

// Standard Dynamic Programming Levenshtein Distance
int levenshtein_distance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    // Allocate 2D matrix on the heap
    int **dp = (int **)malloc((len1 + 1) * sizeof(int *));
    for (int i = 0; i <= len1; i++) {
        dp[i] = (int *)malloc((len2 + 1) * sizeof(int));
    }
    
    // Initialize base cases
    for (int i = 0; i <= len1; i++) dp[i][0] = i;
    for (int j = 0; j <= len2; j++) dp[0][j] = j;
    
    // Fill the matrix
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (tolower((unsigned char)s1[i - 1]) == tolower((unsigned char)s2[j - 1])) ? 0 : 1;
            dp[i][j] = min_of_three(
                dp[i - 1][j] + 1,       // Deletion
                dp[i][j - 1] + 1,       // Insertion
                dp[i - 1][j - 1] + cost  // Substitution
            );
        }
    }
    
    int result = dp[len1][len2];
    
    // Free matrix memory
    for (int i = 0; i <= len1; i++) {
        free(dp[i]);
    }
    free(dp);
    
    return result;
}

// Normalized Levenshtein Distance (returns 0.0 for exact matches, up to 1.0 for completely different strings)
float normalized_levenshtein(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    if (len1 == 0 && len2 == 0) return 0.0f;
    int max_len = (len1 > len2) ? len1 : len2;
    int dist = levenshtein_distance(s1, s2);
    return (float)dist / max_len;
}

// Check if a target string contains a word that is within 30% Normalized Levenshtein distance of the query word
bool is_word_fuzzy_match(const char *str, const char *query_word) {
    if (!str || !query_word) return false;
    int len_qw = strlen(query_word);
    if (len_qw == 0) return true;

    int len_str = strlen(str);
    char temp_word[128];
    int temp_idx = 0;

    for (int i = 0; i <= len_str; i++) {
        char c = str[i];
        if (isalnum((unsigned char)c)) {
            if (temp_idx < (int)sizeof(temp_word) - 1) {
                temp_word[temp_idx++] = c;
            }
        } else {
            if (temp_idx > 0) {
                temp_word[temp_idx] = '\0';
                
                // Calculate Normalized Levenshtein
                float n_dist = normalized_levenshtein(temp_word, query_word);
                if (n_dist <= 0.30f) {
                    return true;
                }
                temp_idx = 0; // reset for next word
            }
        }
    }
    return false;
}

// ==========================================
// 2. STATE-MACHINE CSV PARSER
// ==========================================

void parse_field(const char **ptr, char *dest, int dest_len) {
    int idx = 0;
    bool in_quotes = false;
    const char *p = *ptr;
    
    // Skip leading spaces
    while (*p == ' ') p++;
    
    if (*p == '"') {
        in_quotes = true;
        p++; // skip starting quote
    }
    
    while (*p != '\0') {
        if (in_quotes) {
            if (*p == '"') {
                if (*(p + 1) == '"') {
                    if (idx < dest_len - 1) {
                        dest[idx++] = '"';
                    }
                    p += 2;
                    continue;
                } else {
                    in_quotes = false;
                    p++; // skip ending quote
                    continue;
                }
            }
        } else {
            if (*p == ',' || *p == '\n' || *p == '\r') {
                if (*p == ',') {
                    p++; // skip comma
                } else {
                    // skip all trailing newline characters
                    while (*p == '\n' || *p == '\r') p++;
                }
                break;
            }
        }
        
        if (idx < dest_len - 1) {
            dest[idx++] = *p;
        }
        p++;
    }
    
    // Trim trailing spaces and newlines
    while (idx > 0 && (dest[idx - 1] == ' ' || dest[idx - 1] == '\r' || dest[idx - 1] == '\n')) {
        idx--;
    }
    
    dest[idx] = '\0';
    *ptr = p;
}

// ==========================================
// 3. CORE PUBLIC API
// ==========================================

Quote* load_quotes(const char *filepath, int *out_count) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        *out_count = 0;
        return NULL;
    }

    char line[2048];
    int count = 0;
    
    // Skip header line
    if (fgets(line, sizeof(line), file)) {
        // counted lines skip header
    }
    
    // First pass: count non-empty records
    while (fgets(line, sizeof(line), file)) {
        if (strlen(line) > 5) {
            count++;
        }
    }

    if (count == 0) {
        fclose(file);
        *out_count = 0;
        return NULL;
    }

    // Allocate dynamic array
    Quote *quotes = (Quote *)malloc(count * sizeof(Quote));
    if (!quotes) {
        fclose(file);
        *out_count = 0;
        return NULL;
    }

    // Second pass: parse line values into structures
    fseek(file, 0, SEEK_SET);
    if (fgets(line, sizeof(line), file)) {
        // Skip header
    }

    int idx = 0;
    while (fgets(line, sizeof(line), file) && idx < count) {
        if (strlen(line) <= 5) continue;
        
        const char *ptr = line;
        char sitcom[MAX_SITCOM_LEN];
        char season_str[16];
        char episode_str[16];
        char speaker[MAX_CHAR_LEN];
        char spoken_to[MAX_CHAR_LEN];
        char context[MAX_CONTEXT_LEN];
        char text[MAX_QUOTE_LEN];

        parse_field(&ptr, sitcom, sizeof(sitcom));
        parse_field(&ptr, season_str, sizeof(season_str));
        parse_field(&ptr, episode_str, sizeof(episode_str));
        parse_field(&ptr, speaker, sizeof(speaker));
        parse_field(&ptr, spoken_to, sizeof(spoken_to));
        parse_field(&ptr, context, sizeof(context));
        parse_field(&ptr, text, sizeof(text));

        // Safely copy to dynamic structures
        strncpy(quotes[idx].sitcom, sitcom, sizeof(quotes[idx].sitcom) - 1);
        quotes[idx].sitcom[sizeof(quotes[idx].sitcom) - 1] = '\0';
        
        quotes[idx].season = atoi(season_str);
        quotes[idx].episode = atoi(episode_str);
        
        strncpy(quotes[idx].speaker, speaker, sizeof(quotes[idx].speaker) - 1);
        quotes[idx].speaker[sizeof(quotes[idx].speaker) - 1] = '\0';
        
        strncpy(quotes[idx].spoken_to, spoken_to, sizeof(quotes[idx].spoken_to) - 1);
        quotes[idx].spoken_to[sizeof(quotes[idx].spoken_to) - 1] = '\0';
        
        strncpy(quotes[idx].context, context, sizeof(quotes[idx].context) - 1);
        quotes[idx].context[sizeof(quotes[idx].context) - 1] = '\0';
        
        strncpy(quotes[idx].text, text, sizeof(quotes[idx].text) - 1);
        quotes[idx].text[sizeof(quotes[idx].text) - 1] = '\0';
        
        idx++;
    }

    fclose(file);
    *out_count = idx;
    return quotes;
}

Quote* search_quotes(const Quote *quotes, int count, const char *keyword, int *out_match_count) {
    if (!quotes || count <= 0) {
        *out_match_count = 0;
        return NULL;
    }

    // Trim leading/trailing spaces from keyword
    char clean_kw[256];
    int kw_idx = 0;
    while (*keyword == ' ') keyword++;
    strncpy(clean_kw, keyword, sizeof(clean_kw) - 1);
    clean_kw[sizeof(clean_kw) - 1] = '\0';
    
    // Trim trailing
    int clean_len = strlen(clean_kw);
    while (clean_len > 0 && clean_kw[clean_len - 1] == ' ') {
        clean_kw[clean_len - 1] = '\0';
        clean_len--;
    }

    // Empty search matches everything
    bool empty_search = (strlen(clean_kw) == 0);

    // Count matches in first pass
    int match_count = 0;
    for (int i = 0; i < count; i++) {
        bool match = false;
        if (empty_search) {
            match = true;
        } else {
            // 1. Check exact case-insensitive substring matches first (fallback)
            if (contains_exact_substring(quotes[i].text, clean_kw) ||
                contains_exact_substring(quotes[i].speaker, clean_kw) ||
                contains_exact_substring(quotes[i].context, clean_kw) ||
                contains_exact_substring(quotes[i].sitcom, clean_kw)) {
                match = true;
            } else {
                // 2. Fall back to Levenshtein fuzzy word matches
                // Split keyword into words and check if ALL search words fuzzy match in any quote fields
                char kw_copy[256];
                strncpy(kw_copy, clean_kw, sizeof(kw_copy) - 1);
                kw_copy[sizeof(kw_copy) - 1] = '\0';
                
                char *token = strtok(kw_copy, " ");
                bool all_tokens_match = true;
                
                while (token != NULL) {
                    bool token_matches_any = 
                        is_word_fuzzy_match(quotes[i].text, token) ||
                        is_word_fuzzy_match(quotes[i].speaker, token) ||
                        is_word_fuzzy_match(quotes[i].context, token) ||
                        is_word_fuzzy_match(quotes[i].sitcom, token);
                    
                    if (!token_matches_any) {
                        all_tokens_match = false;
                        break;
                    }
                    token = strtok(NULL, " ");
                }
                
                if (all_tokens_match && strlen(clean_kw) > 0) {
                    match = true;
                }
            }
        }
        
        if (match) {
            match_count++;
        }
    }

    if (match_count == 0) {
        *out_match_count = 0;
        return NULL;
    }

    // Allocate memory for matching array
    Quote *matches = (Quote *)malloc(match_count * sizeof(Quote));
    if (!matches) {
        *out_match_count = 0;
        return NULL;
    }

    // Populate matches array in second pass
    int idx = 0;
    for (int i = 0; i < count; i++) {
        bool match = false;
        if (empty_search) {
            match = true;
        } else {
            if (contains_exact_substring(quotes[i].text, clean_kw) ||
                contains_exact_substring(quotes[i].speaker, clean_kw) ||
                contains_exact_substring(quotes[i].context, clean_kw) ||
                contains_exact_substring(quotes[i].sitcom, clean_kw)) {
                match = true;
            } else {
                char kw_copy[256];
                strncpy(kw_copy, clean_kw, sizeof(kw_copy) - 1);
                kw_copy[sizeof(kw_copy) - 1] = '\0';
                
                char *token = strtok(kw_copy, " ");
                bool all_tokens_match = true;
                
                while (token != NULL) {
                    bool token_matches_any = 
                        is_word_fuzzy_match(quotes[i].text, token) ||
                        is_word_fuzzy_match(quotes[i].speaker, token) ||
                        is_word_fuzzy_match(quotes[i].context, token) ||
                        is_word_fuzzy_match(quotes[i].sitcom, token);
                    
                    if (!token_matches_any) {
                        all_tokens_match = false;
                        break;
                    }
                    token = strtok(NULL, " ");
                }
                
                if (all_tokens_match && strlen(clean_kw) > 0) {
                    match = true;
                }
            }
        }

        if (match && idx < match_count) {
            matches[idx++] = quotes[i];
        }
    }

    *out_match_count = idx;
    return matches;
}

// ==========================================
// 4. CUSTOM POINTER-BASED SORTING ALGORITHMS
// ==========================================

int compare_quotes(const Quote *a, const Quote *b, SortType sort_type) {
    if (sort_type == CHRONOLOGICAL) {
        int sitcom_cmp = case_insensitive_compare(a->sitcom, b->sitcom);
        if (sitcom_cmp != 0) {
            return sitcom_cmp;
        }
        if (a->season != b->season) {
            return a->season - b->season;
        }
        return a->episode - b->episode;
    } else if (sort_type == SPEAKER) {
        int speaker_cmp = case_insensitive_compare(a->speaker, b->speaker);
        if (speaker_cmp != 0) {
            return speaker_cmp;
        }
        
        // Stable chronological sorting fallback for matching speaker names
        int sitcom_cmp = case_insensitive_compare(a->sitcom, b->sitcom);
        if (sitcom_cmp != 0) {
            return sitcom_cmp;
        }
        if (a->season != b->season) {
            return a->season - b->season;
        }
        return a->episode - b->episode;
    }
    return 0;
}

void swap_quotes(Quote *a, Quote *b) {
    Quote temp = *a;
    *a = *b;
    *b = temp;
}

// Custom Pointer-Based Bubble Sort
void quote_bubble_sort(Quote *start, const Quote *end, SortType sort_type) {
    if (start < end) {
        int n = end - start;
        for (int j = 0; j < n; j++) {
            bool swapped = false;
            for (Quote *i = start; i < end; i++) {
                if (compare_quotes(i, i + 1, sort_type) > 0) {
                    swap_quotes(i, i + 1);
                    swapped = true;
                }
            }
            if (!swapped) break;
        }
    }
}

// Custom Pointer-Based Partitioning Helper
Quote* partition_quotes(Quote *start, Quote *end, SortType sort_type) {
    Quote *i = start;
    for (Quote *j = start; j < end; j++) {
        if (compare_quotes(j, end, sort_type) < 0) {
            swap_quotes(i, j);
            i++;
        }
    }
    swap_quotes(i, end);
    return i;
}

// Custom Pointer-Based Quick Sort
void quote_quick_sort(Quote *start, Quote *end, SortType sort_type) {
    if (start < end) {
        Quote *pi = partition_quotes(start, end, sort_type);
        quote_quick_sort(start, pi - 1, sort_type);
        quote_quick_sort(pi + 1, end, sort_type);
    }
}

void sort_quotes(Quote *start, Quote *end, SortType sort_type, SortAlgorithm algo) {
    if (!start || !end || start >= end) return;
    if (algo == BUBBLE_SORT) {
        quote_bubble_sort(start, end, sort_type);
    } else if (algo == QUICK_SORT) {
        quote_quick_sort(start, end, sort_type);
    }
}

// ==========================================
// 5. MEMORY MANAGEMENT
// ==========================================

void free_quotes(Quote *quotes) {
    if (quotes) {
        free(quotes);
    }
}
