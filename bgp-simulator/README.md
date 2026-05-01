# BGP Simulator

A Border Gateway Protocol (BGP) route propagation simulator built in C++ and compiled to WebAssembly for use in the browser. It models the Gao-Rexford policy framework using real CAIDA AS relationship data, supports Route Origin Validation (ROV), and visualizes the resulting AS-level routing graph with D3.js.

**Live demo:** [https://bgp-sim.lakshita1515.workers.dev](https://bgp-sim.lakshita1515.workers.dev)

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [How It Works](#how-it-works)
  - [Input Files](#input-files)
  - [Propagation Pipeline](#propagation-pipeline)
  - [Gao-Rexford Rules](#gao-rexford-rules)
  - [Route Origin Validation (ROV)](#route-origin-validation-rov)
- [Web Interface](#web-interface)
- [Building](#building)
  - [Native CLI Build](#native-cli-build)
  - [WebAssembly Build](#webassembly-build)
- [CLI Usage](#cli-usage)
- [Running Tests](#running-tests)
- [Output Format](#output-format)
- [Architecture](#architecture)
  - [C++ Backend](#c-backend)
  - [Frontend](#frontend)
- [File Reference](#file-reference)

---

## Overview

BGP is the routing protocol that governs how traffic is forwarded across the internet between Autonomous Systems (ASes) — the independently operated networks run by ISPs, universities, cloud providers, etc. Each AS announces the IP prefixes it owns, and neighboring ASes propagate those announcements according to their routing policies.

This simulator models that process. Given a snapshot of the real AS topology (from CAIDA) and a set of prefix announcements, it runs the full three-phase Gao-Rexford propagation and produces a table of which AS learned which route. The entire simulation runs in the browser via WebAssembly — no server-side computation required.

---

## Features

- Full three-phase BGP propagation (bottom-up → peer → top-down)
- Gao-Rexford route selection (customer > peer > provider preference)
- AS path tracking with prepending at each hop
- Route Origin Validation (ROV) support — ROV ASes reject `rov_invalid` routes
- Cycle detection on the provider/customer graph before propagation
- Real CAIDA AS relationship data included (`20260401.as-rel2.txt`)
- Browser-based simulation via Emscripten WebAssembly
- D3.js force-directed graph visualization of propagation results
- CSV download of simulation output
- Native CLI build for offline/scripted use

---

## Project Structure

```
bgp-simulator/
├── include/                  # C++ header files
│   ├── announcement.hpp      # Announcement struct + FROM_* constants
│   ├── as.hpp                # AS struct (asn, customers, peers, providers, policy)
│   ├── bgp_policy.hpp        # BGP policy class (local_rib, received_queue)
│   ├── graph.hpp             # Graph struct + function declarations
│   ├── policy.hpp            # Abstract base Policy class
│   ├── relationships.hpp     # PROVIDER_TO_CUSTOMER / PEER_TO_PEER constants
│   ├── rov_policy.hpp        # ROV subclass of BGP
│   ├── simulator.hpp         # propagateAnnouncements() declaration
│   └── utils.hpp             # misc utilities
│
├── src/                      # C++ source files
│   ├── main.cpp              # Native CLI entry point
│   ├── wasm_entry.cpp        # WebAssembly entry point (Emscripten)
│   ├── graph_loader.cpp      # Parses CAIDA AS relationship file
│   ├── cycle_detection.cpp   # DFS cycle detection on provider/customer graph
│   ├── propagation_rank.cpp  # Kahn's algorithm to compute propagation order
│   ├── simulator.cpp         # Three-phase BGP propagation logic
│   ├── csv_writer.cpp        # Serializes local RIBs to CSV
│   ├── rov_loader.cpp        # Reads ROV ASN list, upgrades policies to ROV
│   └── announcement_loader.cpp # Reads announcements CSV, seeds origin ASes
│
├── tests/                    # Unit tests
│   ├── test_graph.cpp        # Graph construction and file parsing tests
│   ├── test_cycle.cpp        # Cycle detection tests
│   ├── test_propagation.cpp  # Propagation, path prepending, tiebreak tests
│   └── test_rov.cpp          # ROV filtering tests
│
├── public/                   # Web frontend
│   ├── index.html            # Main HTML page
│   ├── app.js                # Frontend JS — loads WASM, runs sim, renders graph
│   ├── bgp_sim.js            # Auto-generated Emscripten WASM glue code
│   ├── styles.css            # Page styles
│   └── 20260401.as-rel2.txt  # CAIDA AS relationship snapshot (April 2026)
│
├── Makefile                  # Native build + test targets
└── emsdk/                    # Emscripten SDK (for WASM compilation)
```

---

## How It Works

### Input Files

#### 1. AS Relationship File (CAIDA format)
Pipe-delimited, one relationship per line. Comments start with `#`.

```
# format: as1|as2|relationship|source
3356|7922|-1|bgp       # AS3356 is provider of AS7922
15169|1299|0|bgp       # AS15169 and AS1299 are peers
```

Relationship codes:
- `-1` — `as1` is the provider of `as2` (PROVIDER_TO_CUSTOMER)
- `0`  — `as1` and `as2` are peers (PEER_TO_PEER)

#### 2. Announcements CSV
Specifies which ASes are origin ASes for which prefixes.

```
asn,prefix,rov_invalid
15169,8.8.0.0/16,False
666,1.2.3.0/24,True
```

- `asn` — the AS originating the prefix
- `prefix` — the IP prefix being announced (CIDR notation)
- `rov_invalid` — `True` if the announcement violates RPKI records (simulates a hijack)

#### 3. ROV ASN List (optional)
A plain text file with one ASN per line. ASes listed here are upgraded to ROV and will reject `rov_invalid` announcements. Lines starting with `#` are ignored.

```
# ROV-enforcing ASes
3356
1299
```

---

### Propagation Pipeline

The simulator runs the following steps in order:

1. **Load graph** — parse the CAIDA relationship file, build bidirectional AS adjacency lists
2. **Cycle check** — run DFS on the provider→customer graph; abort if a cycle is found (propagation would loop forever)
3. **Apply ROV** — upgrade listed ASes from BGP to ROV policy (must happen before announcements arrive)
4. **Seed announcements** — install origin routes directly into each origin AS's local RIB
5. **Compute propagation ranks** — Kahn's topological sort assigns a rank (0 = leaf customer, N = top provider) to each AS
6. **Propagate** — three-phase Gao-Rexford propagation (see below)
7. **Write output** — serialize every AS's local RIB to CSV

---

### Gao-Rexford Rules

BGP route selection follows three tiebreak rules applied in order:

| Priority | Rule | Reason |
|----------|------|--------|
| 1st | `FROM_CUSTOMER` > `FROM_PEER` > `FROM_PROVIDER` | Customer routes earn money; provider routes cost money |
| 2nd | Shorter AS path | Fewer hops = more efficient |
| 3rd | Lower `next_hop_asn` | Arbitrary but deterministic tiebreak |

**Export rules** (what an AS is allowed to advertise to whom):
- Customer routes → advertised to providers, peers, and customers
- Peer routes → advertised to customers only
- Provider routes → advertised to customers only

The three propagation phases implement this:
- **Phase 1 (bottom-up):** Customers send their full RIB to providers, processed rank by rank from 0 up to N
- **Phase 2 (peer):** All ASes exchange routes with their peers simultaneously
- **Phase 3 (top-down):** Providers send their full RIB to customers, processed from rank N down to 0

---

### Route Origin Validation (ROV)

ROV is a security mechanism that allows an AS to reject route announcements whose origin ASN doesn't match the registered RPKI prefix-origin pair. In this simulator:

- The `rov_invalid` field on an announcement simulates an RPKI-invalid route (e.g. a prefix hijack)
- A plain **BGP** AS accepts and propagates all announcements regardless of `rov_invalid`
- An **ROV** AS silently drops any announcement with `rov_invalid = true` on receipt — the route never enters its queue and is never installed or forwarded

To mark ASes as ROV-enforcing, include their ASNs in the ROV input file.

---

## Web Interface

The live demo is deployed at: **[https://bgp-sim.lakshita1515.workers.dev](https://bgp-sim.lakshita1515.workers.dev)**

### How to use it

1. **AS Relationship File** — paste or upload a CAIDA-format AS relationship file. The included `20260401.as-rel2.txt` snapshot is pre-loaded by default.
2. **Announcements CSV** — upload or paste a CSV with columns `asn,prefix,rov_invalid`.
3. **ROV ASNs (optional)** — upload a text file with one ASN per line to enable ROV filtering.
4. Click **Run Simulation** — the WASM module runs the full propagation in-browser.
5. The **results graph** renders below using D3.js (force-directed layout). Nodes are ASes; edges show which AS learned a route from which neighbor.
6. Click **Download CSV** to save the full propagation results.

### Legend
- **Blue nodes** — origin ASes (announced a prefix)
- **Grey nodes** — transit ASes (learned routes via propagation)
- **Edge direction** — indicates the direction a route traveled

---

## Building

### Native CLI Build

Requires a C++11-compatible compiler (GCC or Clang).

```bash
cd bgp-simulator

# build the native binary
make

# this produces: ./bgp_sim
```

To set compiler flags:
```bash
make CXX=g++ CXXFLAGS="-std=c++11 -O2"
```

---

### WebAssembly Build

Requires the Emscripten SDK (included in `emsdk/`).

```bash
cd bgp-simulator

# activate the Emscripten environment
source ./emsdk/emsdk_env.sh

# compile to WebAssembly
em++ -std=c++11 -O2 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=BGPSimModule \
  -sENVIRONMENT=web \
  -sFILESYSTEM=0 \
  --bind \
  -Iinclude \
  src/graph_loader.cpp \
  src/cycle_detection.cpp \
  src/propagation_rank.cpp \
  src/simulator.cpp \
  src/csv_writer.cpp \
  src/rov_loader.cpp \
  src/announcement_loader.cpp \
  src/wasm_entry.cpp \
  -o public/bgp_sim.js
```

This produces `public/bgp_sim.js` (JS glue) and `public/bgp_sim.wasm` (the compiled module).

---

## CLI Usage

```
./bgp_sim <as-rel2-file> <anns.csv> <rov_asns.txt> <output.csv>
```

| Argument | Description |
|----------|-------------|
| `as-rel2-file` | CAIDA AS relationship file |
| `anns.csv` | Announcements CSV (`asn,prefix,rov_invalid`) |
| `rov_asns.txt` | ROV ASN list (can be an empty file) |
| `output.csv` | Where to write propagation results |

Example:
```bash
./bgp_sim 20260401.as-rel2.txt announcements.csv rov_asns.txt results.csv
```

---

## Running Tests

```bash
make test
```

This compiles and runs four test binaries:

| Test binary | What it tests |
|-------------|---------------|
| `test_graph` | Graph construction, file parsing, bidirectional relationship wiring |
| `test_cycle` | Cycle detection — valid DAG, actual cycle, peer edges ignored |
| `test_propagation` | All three propagation phases, AS path prepending, all Gao-Rexford tiebreaks, CSV output |
| `test_rov` | ROV filtering on receive, BGP accepting invalid routes, ROV in full propagation |

To clean up build artifacts and test binaries:
```bash
make clean
```

---

## Output Format

The simulation output is a CSV with one row per (AS, prefix) pair where the AS has a route in its local RIB after propagation.

```
asn,prefix,as_path
15169,8.8.0.0/16,"(15169,)"
3356,8.8.0.0/16,"(3356, 15169,)"
7922,8.8.0.0/16,"(7922, 3356, 15169,)"
```

- `asn` — the AS that has this route
- `prefix` — the IP prefix
- `as_path` — the full path from this AS back to the origin, as a quoted Python-style tuple

Note: single-hop paths use a trailing comma (e.g. `"(15169,)"`) to match Python tuple formatting.

---

## Architecture

### C++ Backend

The backend is a header-only data model with a set of loosely coupled compilation units:

```
graph_loader   ──► Graph (AS nodes + adjacency lists)
                        │
                        ├── cycle_detection   (DFS, aborts if cycle found)
                        ├── rov_loader        (upgrades BGP → ROV for listed ASes)
                        ├── announcement_loader (seeds local_rib of origin ASes)
                        ├── propagation_rank  (Kahn's BFS → rank per AS)
                        └── simulator         (three-phase propagation)
                                │
                                └── csv_writer / wasm_entry  (serialize results)
```

**Policy hierarchy:**
```
Policy (abstract)
  └── BGP
        └── ROV   (overrides receiveAnnouncement to drop rov_invalid routes)
```

Each AS holds a `Policy*` pointer. The simulator calls `receiveAnnouncement()` polymorphically so ROV filtering is transparent to the propagation logic.

### Frontend

- `index.html` — single-page layout with a two-column grid (inputs left, visualization right)
- `app.js` — ES module; loads `BGPSimModule`, handles file uploads, calls `runSimulation()`, parses the CSV result, builds a D3 force-directed graph
- `bgp_sim.js` — auto-generated Emscripten glue code; exports the `BGPSimModule` promise-based factory
- `styles.css` — page styling

The JavaScript calls into WASM via a single function exposed through Emscripten's `embind`:
```javascript
const result = module.runSimulation(asRelString, annsString, rovString);
// returns a CSV string, or "ERROR: ..." on failure
```

---

## File Reference

| File | Purpose |
|------|---------|
| `src/main.cpp` | Native CLI entry point |
| `src/wasm_entry.cpp` | WebAssembly entry point; in-memory versions of all file parsers |
| `src/graph_loader.cpp` | Parses CAIDA pipe-delimited file into `Graph` |
| `src/cycle_detection.cpp` | DFS-based cycle detection on provider→customer edges |
| `src/propagation_rank.cpp` | Kahn's topological sort, assigns rank levels to ASes |
| `src/simulator.cpp` | Three-phase Gao-Rexford propagation |
| `src/csv_writer.cpp` | Writes local RIBs to CSV after propagation |
| `src/rov_loader.cpp` | Reads ROV ASN list, replaces BGP policies with ROV |
| `src/announcement_loader.cpp` | Reads announcements CSV, seeds origin AS local RIBs |
| `include/announcement.hpp` | `Announcement` struct + `FROM_CUSTOMER/PEER/PROVIDER` constants |
| `include/as.hpp` | `AS` struct (asn, customers, peers, providers, policy pointer) |
| `include/bgp_policy.hpp` | `BGP` class with `local_rib` and `received_queue` |
| `include/rov_policy.hpp` | `ROV` subclass that drops `rov_invalid` announcements |
| `include/graph.hpp` | `Graph` struct (`map<int, AS>`) + function declarations |
| `include/policy.hpp` | Abstract `Policy` base class |
| `include/relationships.hpp` | `PROVIDER_TO_CUSTOMER` and `PEER_TO_PEER` constants |
| `public/index.html` | Web UI HTML |
| `public/app.js` | Frontend JS logic |
| `public/bgp_sim.js` | Emscripten-generated WASM glue |
| `public/styles.css` | CSS styles |
| `public/20260401.as-rel2.txt` | CAIDA AS relationship snapshot (April 2026) |
| `tests/test_graph.cpp` | Graph construction unit tests |
| `tests/test_cycle.cpp` | Cycle detection unit tests |
| `tests/test_propagation.cpp` | Propagation and tiebreak unit tests |
| `tests/test_rov.cpp` | ROV filtering unit tests |
