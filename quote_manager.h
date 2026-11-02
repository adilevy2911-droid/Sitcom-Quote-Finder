#ifndef QUOTE_MANAGER_H
#define QUOTE_MANAGER_H

#define MAX_SITCOM_LEN 64
#define MAX_CHAR_LEN 64
#define MAX_CONTEXT_LEN 256
#define MAX_QUOTE_LEN 512

typedef struct Quote {
    char sitcom[MAX_SITCOM_LEN];
    int season;
    int episode;
    char speaker[MAX_CHAR_LEN];
    char spoken_to[MAX_CHAR_LEN];
    char context[MAX_CONTEXT_LEN];
    char text[MAX_QUOTE_LEN];
} Quote;

typedef enum SortType {
    CHRONOLOGICAL = 0, // Sorts by Sitcom Name, then Season, then Episode
    SPEAKER = 1        // Sorts alphabetically by Character Who Said It
} SortType;

typedef enum SortAlgorithm {
    BUBBLE_SORT = 0,
    QUICK_SORT = 1
} SortAlgorithm;

// Public Dynamic API loaded via ctypes
Quote* load_quotes(const char *filepath, int *out_count);
Quote* search_quotes(const Quote *quotes, int count, const char *keyword, int *out_match_count);
void sort_quotes(Quote *start, Quote *end, SortType sort_type, SortAlgorithm algo);
void free_quotes(Quote *quotes);

#endif // QUOTE_MANAGER_H
