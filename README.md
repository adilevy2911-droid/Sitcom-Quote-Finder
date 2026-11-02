# Sitcom Quote Finder

An end-to-end, high-performance web platform that interfaces a **Python (Flask)** web server with a custom **C algorithmic engine** via `ctypes` bindings. It enables users to search and filter 80 iconic quotes from legendary sitcoms (*Friends, How I Met Your Mother, Brooklyn Nine-Nine, Gilmore Girls, and New Girl*) using dynamic Levenshtein edit distance metrics and an adaptive backend caching layer.

---

## Dashboard & Features Preview

<p align="center">
  <h3>1. Main Interface & Catalog</h3>
  <img src="https://github.com/user-attachments/assets/7886b2e9-9e73-48ce-82e8-cb726c2ed47c" width="600" alt="Main Interface" />
  <br>
  <sub><em>Main UI displaying quotes categorized by sitcoms with dynamic filtering options.</em></sub>
</p>

<hr>

<p align="center">
  <h3>2. Fuzzy Search & Real-Time Telemetry HUD</h3>
  <img src="https://github.com/user-attachments/assets/faad6e87-62d7-4965-a9dd-7ffbed75063d" width="600" alt="Fuzzy Search" />
  <br>
  <sub><em>C-powered Levenshtein distance engine handling typos with live performance telemetry.</em></sub>
</p>

<hr>

<p align="center">
  <h3>3. Direct YouTube Scene Integration</h3>
  <img src="https://github.com/user-attachments/assets/55cd14b9-8fa0-4b49-9e49-f0e34a391e2c" width="600" alt="YouTube Integration" />
  <br>
  <sub><em>One-click button redirecting directly to the specific sitcom scene query on YouTube.</em></sub>
</p>
---

## Project Architecture & Features

- **Levenshtein Edit Distance Matrix:** Dynamically computes string transformation cost matrices inside the C engine. Supports flexible, fault-tolerant query execution with up to a 30% normalized character error rate to catch user typos seamlessly.
- **Adaptive Hybrid Sorting Dispatcher:**
  - **N ≤ 10:** Fires a local pointer-based **Bubble Sort** to eliminate recursive context-switching and stack frame allocations.
  - **N > 10:** Triggers a pointer-based **Quick Sort** optimizing median partitions to sustain an average performance of O(n log n).
- **O(1) LRU Caching System:** A Python-implemented Least Recently Used (LRU) data structure intercepts inbound repetitive search parameters, delivering instant cache hits and avoiding redundant C matrix recalculations.
- **YouTube Scene Integration:** One-click button on each quote result that dynamically launches a targeted search for the exact sitcom scene on YouTube.
---

## Repository Directory Structure

```text
.
├── app.py              # Flask server backend featuring LRU Cache & sorting dispatcher
├── quote_manager.c     # Levenshtein engine, dynamic sizing arrays & custom sorting pivots
├── quote_manager.h     # Structural maps for string telemetry arrays
├── SITCOM_DATA.csv     # Master text database housing 80 situational quotes
└── templates/
    └── index.html      # Single-page interface tracking runtime telemetry HUD
```

---

## Tech Stack

- **Target Environment:** Linux / WSL (Ubuntu)
- **Languages & Frameworks:** C (GCC Engine) / Python 3.12 (Flask) / HTML5 & CSS3
- **Interoperability:** `ctypes` C-Python Bindings

---

## Execution & Local Verification

### Prerequisites
- Linux environment or **WSL** (Windows Subsystem for Linux)
- `gcc` compiler
- `python3` and `pip`

### Setup & Execution

1. **Clone the repository and enter the directory:**
   ```bash
   git clone [https://github.com/adilevy2911-droid/Sitcom-Quote-Finder.git](https://github.com/adilevy2911-droid/Sitcom-Quote-Finder.git)
   cd Sitcom-Quote-Finder
   ```

2. **Compile the dynamic shared C library:**
   ```bash
   gcc -shared -o libquote_manager.so -fPIC quote_manager.c
   ```

3. **Install required Python dependencies:**
   ```bash
   pip install flask
   ```

4. **Launch the web server:**
   ```bash
   python3 app.py
   ```

5. **Access the application:**
   Open your browser and navigate to `http://127.0.0.1:5000`
