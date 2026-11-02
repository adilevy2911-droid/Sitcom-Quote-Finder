import ctypes
import os
import time
from flask import Flask, request, jsonify, render_template

# Initialize Flask
app = Flask(__name__)

# ==========================================
# 1. CTYPES CONFIGURATION & BINDINGS
# ==========================================

MAX_SITCOM_LEN = 64
MAX_CHAR_LEN = 64
MAX_CONTEXT_LEN = 256
MAX_QUOTE_LEN = 512

class QuoteC(ctypes.Structure):
    _fields_ = [
        ("sitcom", ctypes.c_char * MAX_SITCOM_LEN),
        ("season", ctypes.c_int),
        ("episode", ctypes.c_int),
        ("speaker", ctypes.c_char * MAX_CHAR_LEN),
        ("spoken_to", ctypes.c_char * MAX_CHAR_LEN),
        ("context", ctypes.c_char * MAX_CONTEXT_LEN),
        ("text", ctypes.c_char * MAX_QUOTE_LEN),
    ]

# Resolve C shared library path
dir_path = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(dir_path, "libquote_manager.so")

if not os.path.exists(lib_path):
    raise FileNotFoundError(f"Shared library not found at: {lib_path}. Please compile it inside WSL first.")

# Load dynamic C library
lib = ctypes.CDLL(lib_path)

# Declare load_quotes binding
lib.load_quotes.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
lib.load_quotes.restype = ctypes.POINTER(QuoteC)

# Declare search_quotes binding
lib.search_quotes.argtypes = [
    ctypes.POINTER(QuoteC), 
    ctypes.c_int, 
    ctypes.c_char_p, 
    ctypes.POINTER(ctypes.c_int)
]
lib.search_quotes.restype = ctypes.POINTER(QuoteC)

# Declare sort_quotes binding
lib.sort_quotes.argtypes = [
    ctypes.POINTER(QuoteC), 
    ctypes.POINTER(QuoteC), 
    ctypes.c_int, 
    ctypes.c_int
]
lib.sort_quotes.restype = None

# Declare free_quotes binding
lib.free_quotes.argtypes = [ctypes.POINTER(QuoteC)]
lib.free_quotes.restype = None

# ==========================================
# 2. MASTER DATASET IN-MEMORY LOADING
# ==========================================

master_count = ctypes.c_int(0)
csv_path = os.path.join(dir_path, "quotes.csv")
master_quotes_ptr = lib.load_quotes(csv_path.encode('utf-8'), ctypes.byref(master_count))

if not master_quotes_ptr or master_count.value == 0:
    raise RuntimeError("Failed to load quotes from quotes.csv. Ensure the file exists and is populated.")

print(f"\033[94m[C ENGINE LOADED]\033[0m Loaded {master_count.value} quotes successfully into dynamic memory.", flush=True)

# Helper to translate QuoteC ctypes arrays into standard Python lists of dicts
def parse_ctypes_quotes(quotes_ptr, count):
    parsed_list = []
    for i in range(count):
        q = quotes_ptr[i]
        parsed_list.append({
            "sitcom": q.sitcom.decode('utf-8', errors='replace'),
            "season": q.season,
            "episode": q.episode,
            "speaker": q.speaker.decode('utf-8', errors='replace'),
            "spoken_to": q.spoken_to.decode('utf-8', errors='replace'),
            "context": q.context.decode('utf-8', errors='replace'),
            "text": q.text.decode('utf-8', errors='replace')
        })
    return parsed_list

# Cache the full parsed master list of dicts for instant loading
master_quotes_list = parse_ctypes_quotes(master_quotes_ptr, master_count.value)

# ==========================================
# 3. CUSTOM LRU CACHE (DOUBLY LINKED LIST + DICTIONARY)
# ==========================================

class Node:
    def __init__(self, key=None, value=None):
        self.key = key
        self.value = value
        self.prev = None
        self.next = None

class LRUCache:
    def __init__(self, capacity=20):
        self.capacity = capacity
        self.cache = {}
        self.head = Node()  # Dummy head
        self.tail = Node()  # Dummy tail
        self.head.next = self.tail
        self.tail.prev = self.head

    def _remove(self, node):
        prev_node = node.prev
        next_node = node.next
        prev_node.next = next_node
        next_node.prev = prev_node

    def _add_to_head(self, node):
        next_node = self.head.next
        self.head.next = node
        node.prev = self.head
        node.next = next_node
        next_node.prev = node

    def get(self, key):
        if key in self.cache:
            node = self.cache[key]
            # Move to head (most recently used)
            self._remove(node)
            self._add_to_head(node)
            return node.value
        return None

    def put(self, key, value):
        if key in self.cache:
            node = self.cache[key]
            node.value = value
            self._remove(node)
            self._add_to_head(node)
        else:
            if len(self.cache) >= self.capacity:
                # Evict tail (least recently used)
                lru_node = self.tail.prev
                self._remove(lru_node)
                del self.cache[lru_node.key]
            new_node = Node(key, value)
            self.cache[key] = new_node
            self._add_to_head(new_node)

# Instantiate search cache (capacity 20 entries)
search_cache = LRUCache(capacity=20)

# ==========================================
# 4. HTTP ROUTING & TELEMETRY API
# ==========================================

@app.route('/')
def index():
    return render_template("index.html")

@app.route('/api/quotes', methods=['GET'])
def get_all_quotes():
    # Returns the full master dataset for initial grid render
    return jsonify({
        "status": "success",
        "count": len(master_quotes_list),
        "quotes": master_quotes_list
    })

@app.route('/api/search', methods=['GET'])
def search_and_sort():
    # Read query params
    keyword = request.args.get('q', '').strip()
    sort_type = request.args.get('sort_type', 'CHRONOLOGICAL') # CHRONOLOGICAL or SPEAKER

    # Map parameters to C enums
    # SortType: CHRONOLOGICAL = 0, SPEAKER = 1
    sort_type_val = 1 if sort_type == 'SPEAKER' else 0

    # Form unique cache key
    cache_key = (keyword.lower(), sort_type)

    # Start timing telemetry
    t_start = time.perf_counter()

    # 1. Query LRU Cache
    cached_payload = search_cache.get(cache_key)
    if cached_payload is not None:
        t_duration = (time.perf_counter() - t_start) * 1000  # Convert to ms
        print(f"\033[92m[CACHE HIT]\033[0m Key: {cache_key} | Fetched from LRU Cache in {t_duration:.4f}ms", flush=True)
        return jsonify({
            "status": "success",
            "source": "cache",
            "time_ms": t_duration,
            "count": len(cached_payload["quotes"]),
            "quotes": cached_payload["quotes"],
            "algo_name": cached_payload["algo_name"],
            "algo_reason": cached_payload["algo_reason"]
        })

    # 2. Cache Miss: Execute C Fuzzy Search Engine
    match_count = ctypes.c_int(0)
    matches_ptr = lib.search_quotes(
        master_quotes_ptr,
        master_count.value,
        keyword.encode('utf-8'),
        ctypes.byref(match_count)
    )

    result_list = []
    algo_name = "Trivial"
    algo_reason = f"Trivial (No sorting needed for N = {match_count.value})"

    if match_count.value > 0 and matches_ptr:
        # Determine sorting algorithm adaptively based on matches N
        if match_count.value > 1:
            if match_count.value <= 10:
                sort_algo_val = 0  # BUBBLE_SORT
                algo_name = "Bubble Sort"
                algo_reason = f"Bubble Sort (Optimized for small N = {match_count.value})"
            else:
                sort_algo_val = 1  # QUICK_SORT
                algo_name = "Quick Sort"
                algo_reason = f"Quick Sort (Optimized for scale, N = {match_count.value})"

            # Sort matched results using C Engine sorting algorithm
            start_ptr = ctypes.pointer(matches_ptr[0])
            end_ptr = ctypes.pointer(matches_ptr[match_count.value - 1])
            lib.sort_quotes(start_ptr, end_ptr, sort_type_val, sort_algo_val)
        else:
            algo_name = "Trivial"
            algo_reason = f"Trivial (No sorting needed for N = {match_count.value})"

        # Parse C structures to standard Python structures
        result_list = parse_ctypes_quotes(matches_ptr, match_count.value)

        # Free C memory allocation for matches
        lib.free_quotes(matches_ptr)

    # 3. Store fresh results and algorithm metrics in LRU Cache
    cached_payload = {
        "quotes": result_list,
        "algo_name": algo_name,
        "algo_reason": algo_reason
    }
    search_cache.put(cache_key, cached_payload)

    # Telemetry response calculation
    t_duration = (time.perf_counter() - t_start) * 1000
    print(f"\033[91m[CACHE MISS]\033[0m Key: {cache_key} | Computed via Levenshtein C Engine in {t_duration:.4f}ms", flush=True)

    return jsonify({
        "status": "success",
        "source": "c_engine",
        "time_ms": t_duration,
        "count": len(result_list),
        "quotes": result_list,
        "algo_name": algo_name,
        "algo_reason": algo_reason
    })

if __name__ == '__main__':
    # Runs on standard port 5000 inside WSL
    app.run(host='0.0.0.0', port=5000, debug=True)
