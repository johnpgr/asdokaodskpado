# **The Orthodox C++ and Compression-Oriented Programming Paradigm: A Comprehensive Technical Analysis**

## **1. Introduction: The Great Schism in Systems Programming**

The landscape of C++ development is currently defined by a profound philosophical schism. On one side stands the "Modern C++" orthodoxy, championed by the ISO standards committee and the broader enterprise software industry. This school of thought prioritizes safety through abstraction, advocating for Resource Acquisition Is Initialization (RAII), smart pointers, extensive template metaprogramming, and the Standard Template Library (STL). On the other side is a growing movement often termed "Orthodox C++," "C-Style C++," or "Performance-Oriented Programming." This counter-movement, significantly popularized by Casey Muratori through *Handmade Hero* and his theoretical writings on "Semantic Compression" and "Clean Code," rejects the modernization of the language in favor of a return to hardware-centric realism.

This report provides an exhaustive analysis of this latter paradigm. It is not merely a collection of coding tips but a fundamental re-evaluation of how software is constructed. The objective is to deconstruct the architectural decisions behind this style, exploring why manual memory management via linear arenas is preferred over reference counting, why unity builds replace complex build systems, and how immediate mode API design simplifies application state management. Furthermore, this document synthesizes these principles into a definitive "LLM Coding Agent Ruleset," designed to guide automated code generation tools in adhering to this rigorous, high-performance standard.

### **1.1 The Theoretical Basis: Hardware Reality vs. Software Abstraction**

The central tenet of the Muratori-style philosophy is that the hardware is the platform, not the language. The computer is not an abstract machine defined by the C++ standard; it is a physical device with specific performance characteristics regarding memory latency, instruction decoding, and cache coherency. Modern C++ abstractions often obscure the reality of how the CPU processes data.<sup>1</sup>

The divergence begins with the treatment of the Central Processing Unit (CPU). In the Modern C++ view, the CPU is an execution engine for the language's semantics. If the language provides a virtual function, the programmer uses it to achieve polymorphism. In the Orthodox view, the CPU is a pipeline that craves predictability. Virtual functions are viewed not just as function calls, but as barriers to code analysis and branch prediction. A virtual call requires a lookup in a vtable, which often causes a cache miss if the object is not "hot" in the cache. Furthermore, the compiler cannot inline these function calls, preventing critical optimizations like loop unrolling or vectorization.<sup>2</sup>

### **1.2 The "Orthodox C++" Subset**

"Orthodox C++" (sometimes facetiously called "C+" or "C minus minus") is defined as a minimal subset of C++ that improves upon C but rejects the complexity of the modern standard.<sup>4</sup> It is described as a "safety clause" for ensuring developers do not use unstable or opaque features.

| Feature | Status in Orthodox C++ | Rationale and Hardware Implication |
| :---- | :---- | :---- |
| **Structs / Classes** | **Accepted** | Used for data grouping. Methods are allowed but often treated as helpers or syntax sugar for C-style functions. Static factory functions (`Type::make()`) are preferred for initialization. |
| **Function Overloading** | **Accepted** | Improves readability (e.g., `math::dot(v2)` vs `math::dot(v3)`) without incurring runtime cost. |
| **Operator Overloading** | **Accepted** | Essential for math types (`Vec3 + Vec3`), allowing mathematical code to read like equations. |
| **Templates** | **Restricted** | Used only for generic containers or math, never for metaprogramming complex logic. Explicit instantiation is preferred. |
| **Exceptions** | **Forbidden** | Hidden control flow, high runtime cost, binary bloat, and interference with simple error handling logic. |
| **RTTI** | **Forbidden** | Unnecessary runtime overhead; use tagged unions or enums instead for type identification. |
| **STL (std::vector, etc.)** | **Forbidden** | Slow compile times, lack of memory control, debug performance overhead, and opaque allocation patterns. |
| **Smart Pointers** | **Forbidden** | RAII obscures ownership; shared_ptr incurs atomic locking costs and creates non-deterministic destruction times. |

This subset is not arbitrary; it is chosen to maximize compilation speed and minimize the "distance" between the source code and the assembly instructions generated.<sup>5</sup>

---

## **2. Philosophy of Code: Semantic Compression**

To understand the specific coding rules of this style, one must first grasp "Semantic Compression," a methodology Casey Muratori proposes as an alternative to traditional Object-Oriented Design (OOD). OOD typically advises programmers to model the problem domain before writing code—creating classes for "Employees" and "Managers" before understanding the specific algorithmic requirements.<sup>7</sup>

### **2.1 The Process of Semantic Compression**

Semantic Compression operates on the principle that the programmer cannot know the correct abstraction until the code is written. It is a bottom-up methodology where architecture is discovered, not planned.

1. **Direct Implementation (Decompression):** The programmer writes the code to solve the immediate problem in the most direct way possible, often resulting in copy-pasted blocks or repetitive logic. For example, if a UI needs five buttons, the programmer writes the drawing code for five buttons sequentially.<sup>7</sup>
2. **Observation:** The programmer observes the code to find exact duplicates in logic.
3. **Compression:** The programmer extracts the commonality into a function or a struct. This is distinct from abstraction; abstraction often hides details, while compression merely factors out repetition without obscuring the underlying operation.<sup>9</sup>

### **2.2 Case Study: The Evolution of *The Witness* UI**

The most cited example of this philosophy is the UI system in *The Witness*. The initial implementation involved manually calculating x and y coordinates for every UI element.

**Stage 1: The Raw Code**

The code began as a procedural list of drawing commands. Variables for layout were managed manually in the local scope.

```cpp
float x = 10.0f;
float y = 100.0f;
draw_button(x, y, "Save");
y += 50.0f; // Manually advancing the cursor
draw_button(x, y, "Load");
y += 50.0f;
```

**Stage 2: Introducing the Layout Struct**

Muratori noticed that x and y were effectively acting as a "stack frame" for the layout operation. He compressed this into a `PanelLayout` struct.

```cpp
struct PanelLayout {
    float at_x;
    float at_y;
    float row_height;
};
```

**Stage 3: Functional Compression**

The repetitive incrementing of y was compressed into a function `layout.row()`. This led to the final API, which is semantically compressed:

```cpp
layout.row();
if (layout.push_button("Save")) { save_game(); }
layout.row();
if (layout.push_button("Load")) { load_game(); }
```

The critical insight here is that `PanelLayout` was not designed as a "Layout Manager Class" upfront. It emerged only after the requirement for managing vertical spacing became repetitive in the actual code.<sup>7</sup>

### **2.3 The Rejection of Premature Reusability**

A core corollary of Semantic Compression is the "Two-Instance Rule." A piece of code should not be reused or abstracted until there are at least two real-world instances of it occurring. This prevents "prematurely reusable" code, which is often cumbersome or incorrect because it was written without enough examples of its actual requirements. Muratori argues that 95% of OO code is "poorly implemented and naively designed" because it attempts to predict future requirements that never materialize, or materialize differently than expected.<sup>11</sup>

---

## **3. Data-Oriented Design: The Structural Foundation**

While Semantic Compression guides the evolution of logic, **Data-Oriented Design (DOD)** guides the structure of memory. This is the implementation of the hardware-centric philosophy.

### **3.1 Structure of Arrays (SoA) vs. Array of Structs (AoS)**

The standard OOP approach uses Array of Structs (AoS). A std::vector\<Entity\> stores Entity objects contiguously. However, if Entity is large (containing position, physics data, AI state, name, inventory), iterating over the vector to update just the position wastes memory bandwidth loading the unused fields into the cache.

DOD favors Structure of Arrays (SoA) or hybrid approaches.

* **AoS:** [(x,y,z,hp,name), (x,y,z,hp,name),...]
* **SoA:** ([x,x,...], [y,y,...], [z,z,...], [hp,hp,...])

In the Muratori style, this often manifests as "hot" vs. "cold" data separation. Entities might be split into EntityHot (position, velocity) and EntityCold (names, description), ensuring that the physics loop only loads the data it strictly needs.<sup>1</sup>

### **3.2 The Switch Statement as Polymorphism**

One of the most controversial aspects of this style is the replacement of virtual functions with switch statements. In a standard OOP Shape example, one would call `shape->area()`, and the vtable would dispatch to `Circle::area()` or `Square::area()`.

The Orthodox approach uses a tagged union or a type enum:

```cpp
enum class ShapeType { Circle, Square };

struct Shape {
    ShapeType type;
    union {
        Circle circle;
        Square square;
    };
};

float get_area(Shape *shape) {
    switch (shape->type) {
        case ShapeType::Circle: return PI * shape->circle.r * shape->circle.r;
        case ShapeType::Square: return shape->square.s * shape->square.s;
    }
    return 0;
}
```

**Hardware Justification:**

1. **Instruction Cache:** The code for `get_area` is contiguous. The CPU loads the function and predicts the branches. In the vtable approach, the code for `Circle::area()` and `Square::area()` might be pages apart in memory.
2. **Inlining:** The compiler can verify the switch and potentially inline the math directly into the loop, allowing for SIMD (Single Instruction, Multiple Data) optimizations that are impossible across a virtual call boundary.<sup>2</sup>

### **3.3 Entity Systems**

In *Handmade Hero*, entities are managed as large arrays of raw data. The system does not "spawn" an object using new; it occupies a slot in a pre-allocated array. This avoids memory fragmentation and ensures that the entity update loop is simply a linear walk through memory. The "Type" of the entity is just an integer, and the update function dispatches behavior based on this integer. This is often referred to as a "tagged union" approach to entity management.<sup>12</sup>

---

## **4. Memory Architecture: The Arena Allocator**

The most technically distinct feature of this coding style is the complete rejection of general-purpose allocators (like malloc and std::allocator) in favor of Region-Based Memory Management, implemented via **Memory Arenas**.

### **4.1 The Cost of General Allocators**

General-purpose allocators (malloc/free) are designed to handle the worst-case scenario: allocations of any size, arriving in any order, and freed in any order. To manage this, they require:

* **Metadata:** Headers and footers for every allocation.
* **Free Lists:** Complex structures to track available holes in memory.
* **Searching:** Algorithms to find the "best fit" block, which takes non-deterministic time.
* **Fragmentation:** Over time, memory becomes fragmented, leading to poor cache locality.<sup>13</sup>

### **4.2 The Arena Implementation**

An Arena (or Linear Allocator) drastically simplifies this. It consists of a pointer to the start of a large block of memory and a "used" offset.

```cpp
struct MemoryArena {
    u8 *base;
    usize size;
    usize used;

    static MemoryArena make(void *buffer, usize size_bytes) {
        MemoryArena result = {};
        result.base = (u8 *)buffer;
        result.size = size_bytes;
        result.used = 0;
        return result;
    }

    void *push_size(usize size_bytes) {
        ASSERT((used + size_bytes) <= size);
        void *result = base + used;
        used += size_bytes;
        return result;
    }
};
```

**Performance Characteristics:**

* **Allocation Cost:** The cost is essentially a single addition operation (incrementing the `used` counter). It is $O(1)$ and takes nanoseconds.
* **Locality:** Objects allocated sequentially in time are guaranteed to be sequential in memory. This is the optimal pattern for the CPU cache.
* **Deallocation:** Individual deallocation is forbidden. Instead, the entire arena is reset by setting `used = 0`. This eliminates the possibility of memory leaks for the lifespan of the arena.<sup>15</sup>

### **4.3 Temporary Memory and Scoping**

For scratch work (e.g., string concatenation, temporary arrays during a frame), the style uses a "Temporary Memory" pattern. This replaces the need for local std::vector variables that allocate and free on the heap.

```cpp
// Begin a temporary scope
TemporaryMemory temp = TemporaryMemory::make(&arena);

// Do allocations
u32 *temp_array = arena.push_array<u32>(1000);
//... heavy processing...

// End scope - resets the arena pointer to where it was
temp.end();
```

This pattern allows for massive amounts of temporary data to be used without fragmentation or the overhead of finding free blocks. It is effectively a "programmable stack".<sup>16</sup>

### **4.4 The "Mental Overhead" Argument**

Critics of this style often argue that manual memory management increases cognitive load. Muratori counters that RAII increases cognitive load by obscuring ownership. With std::shared_ptr, the lifetime of an object is non-deterministic—it depends on the last pointer disappearing. With Arenas, the lifetime is explicit: "This object lives as long as the `level_arena` lives." This makes reasoning about object lifecycles trivial and eliminates "use-after-free" bugs relative to complex pointer graphs.<sup>12</sup>

---

## **5. The Build System: Unity Builds**

The Orthodox C++ style rejects modern build systems like CMake or MSBuild in favor of a simple batch script and a "Unity Build" (Single Translation Unit) structure.

### **5.1 The Unity Build Architecture**

In a standard C++ project, every .cpp file is compiled separately. If 50 files include \<windows.h\>, the compiler parses \<windows.h\> 50 times.

In a Unity Build, a single source file (e.g., build.cpp) includes all other source files:

```cpp
// build.cpp
#include "header.h"
#include "main.cpp"
#include "player.cpp"
#include "renderer.cpp"
```

The compiler is invoked only on build.cpp.

### **5.2 Performance Implications**

1. **Parse Time:** Header files are parsed once. This typically reduces compile times from minutes to seconds. *Handmade Hero*, a project with hundreds of thousands of lines of code, compiles in under 2 seconds.<sup>18</sup>
2. **Optimization:** Because the compiler sees the entire program as one unit, it can perform global optimizations (like inlining functions from player.cpp into main.cpp) without needing expensive Link Time Optimization (LTO) steps.<sup>19</sup>

### **5.3 The build.bat**

The build process is demystified by using a simple shell script. There is no dependency graph to resolve; the script simply calls the compiler.

Example build.bat Breakdown:20

```bat
@echo off
mkdir build
pushd build

REM Load Visual Studio environment variables if not already loaded
call "C:\\Program Files (x86)\\...\\vcvarsall.bat" x64

REM Compiler Flags:
REM -Zi: Debug info
REM -Od: No optimization (Debug)
REM -FC: Full path in diagnostics
REM -nologo: Quiet mode
cl -Zi -Od -FC -nologo..\\code\\win32_handmade.cpp user32.lib gdi32.lib

popd
```

This script is understandable by any programmer and removes the "black box" nature of build tools. It also enforces the discipline that the project *must* be buildable via a simple command line, ensuring portability and simplicity.<sup>22</sup>

---

## **6. LLM Coding Agent Ruleset**

This section consolidates the findings into a formal ruleset for an LLM coding agent. This document is intended to be used as a system prompt or a reference guide for generating code in the Casey Muratori / Orthodox C++ style.

---

### **Artifact: The Orthodox C++ Coding Agent Guidelines**

**Role Definition:**

You are an expert systems programmer adhering to the "Orthodox C++" and "Handmade" philosophy. You reject modern C++ abstractions (RAII, STL, Exceptions) in favor of explicit control flow, linear memory allocation, and data-oriented design. Your goal is to produce code that is high-performance, compiles instantly, and is transparent in its execution.

#### **I. Core Philosophy & Mindset**

* **Hardware First:** Assume the code runs on a real CPU with caches and pipelines. Avoid patterns that cause cache misses (pointer chasing) or pipeline stalls (unpredictable branches).
* **Semantic Compression:** Do not design general classes upfront. Write specific code first, then extract commonality only when duplication occurs.
* **Demystification:** Avoid "magic" code (hidden constructors, destructors, complex templates). The code should do exactly what it says it does.

#### **II. Memory Management Guidelines**

* **Rule 1: No General Allocators.** Never use new, delete, malloc, or free for game objects.
* **Rule 2: Use Arenas.** All memory must be allocated from a MemoryArena.
  * **Context:** Pass `MemoryArena *arena` to functions that need to allocate.
  * **Mechanism:** Use `arena->push_struct<Type>()` and `arena->push_array<Type>(count)` methods.
* **Rule 3: Explicit Lifetimes.** Use `TemporaryMemory::make()` and `end()` for scoped allocations. Never rely on destructors to clean up resources.
* **Rule 4: Pointers over References.** Use pointers (`Type *`) for optional or mutable data. Use References only when syntactically necessary for operator overloading. Pointers are explicit about the potential for null.

#### **III. Data Structure Guidelines**

* **Rule 5: PODs (Plain Old Data).** Structs should primarily contain data. Methods on structs are permitted but should be viewed as syntax sugar. Prefer free functions: `update_player(Player *p)` over `p->update()`.
* **Rule 6: Static Factory Functions.** Use static `make()` functions for struct initialization instead of constructors.
  ```cpp
  struct MemoryArena {
      u8 *base;
      usize size;
      usize used;

      static MemoryArena make(void *buffer, usize size_bytes) {
          MemoryArena result = {};
          result.base = (u8 *)buffer;
          result.size = size_bytes;
          result.used = 0;
          return result;
      }
  };

  // Usage:
  MemoryArena arena = MemoryArena::make(buffer, size);
  ```
  This pattern is explicit, avoids hidden constructor magic, and makes initialization order clear.
* **Rule 7: No STL.** Strictly forbidden: std::vector, std::string, std::map, std::unique_ptr.
  * **Alternative:** Use fixed-size arrays where possible. Use arena-allocated dynamic arrays (`Type *data; int count;`) otherwise.
* **Rule 8: Strings.** Use a custom string view struct:
  ```cpp
  struct String { u8 *data; s64 count; };
  #define STR(s) { (u8 *)s, sizeof(s)-1 }
  ```
  Do not assume null-termination. Do not allocate strings on the heap implicitly.

#### **IV. Control Flow & Error Handling**

* **Rule 9: No Exceptions.** Never use try, catch, or throw.
* **Rule 10: Assertions.** Use `ASSERT()` liberally for programmer errors (impossible conditions). Fail fast.
* **Rule 11: Return Values.** For runtime errors (I/O, Network), return a status code or a result struct.
  * *Pattern:* `internal bool32 load_file(char *filename, FileContents *result)`
* **Rule 12: Switch over Virtuals.** Use enum types and switch statements for polymorphism. Group behavior by operation, not by type.

#### **V. Style & Formatting**

* **Naming (Rust-style):**
  * **Types:** PascalCase (e.g., `GameState`, `SimEntity`, `MemoryArena`).
  * **Functions/Methods:** snake_case (e.g., `push_struct`, `update_and_render`, `make`).
  * **Variables:** snake_case (e.g., `entity_count`, `base`, `size_bytes`).
  * **Constants:** UPPER_SNAKE_CASE (e.g., `MAX_ENTITIES`, `PI`).
  * **Macros:** UPPER_SNAKE_CASE (e.g., `ASSERT`, `KILOBYTES`).
  * **Enum Variants:** PascalCase (e.g., `EntityType::Hero`, `EntityType::Monster`).
* **File Structure:**
  * Use .h for struct definitions and macros.
  * Use .cpp for implementations.
  * **Unity Build:** All .cpp files are #included into a single platform layer file. Do not create separate compilation units.

#### **VI. Example: Correct vs. Incorrect**

**Incorrect (Modern C++):**

```cpp
class Entity {
public:
    virtual void Update() = 0;
    virtual ~Entity() {}
};

class Hero : public Entity {
    std::string name;
    std::vector<Item> inventory;
public:
    void Update() override {... }
};

void Game::Loop() {
    for (auto& e : entities) e->Update(); // Virtual call, cache miss
}
```

**Correct (Orthodox C++):**

```cpp
enum class EntityType { Hero, Monster };

struct Entity {
    EntityType type;
    v3 p; // Position
    String name;
    // Data is inline or handled via IDs
};

void update_and_render(GameState *state) {
    for (int i = 0; i < state->entity_count; ++i) {
        Entity *e = state->entities + i;
        switch (e->type) {
            case EntityType::Hero: update_hero(e); break;
            case EntityType::Monster: update_monster(e); break;
        }
    }
}
```

---

## **7. Detailed Analysis of Key Technical Patterns**

### **7.1 String Handling: The String Struct**

The rejection of std::string is central to the Orthodox style. std::string manages its own memory, often performs a small allocation on the heap (unless Short String Optimization applies), and ensures null-termination. This behavior is "opaque."

**The Custom String Approach:**

The Orthodox string is a "fat pointer" or "slice"—a pointer and a length.

* **Slicing:** To take a substring, one simply advances the pointer and decreases the length. No memory is allocated. This makes parsers extremely fast and memory-efficient.<sup>24</sup>
* **Immutability:** These strings are typically views into a larger buffer (like a loaded file). Modification is done by creating a new string in an Arena, not by mutating the view.
* **UTF-8:** The style strictly prefers UTF-8. char is treated as a byte. Iteration over characters requires decoding the UTF-8 sequence, but random access to bytes is $O(1)$. This avoids the pitfalls of std::wstring and Windows' wchar_t.<sup>26</sup>

### **7.2 Immediate Mode GUI (IMGUI)**

Casey Muratori is widely credited with popularizing the Immediate Mode GUI pattern. This pattern is essential for tools development in the Orthodox style.

**Concept:**

In a Retained Mode GUI (standard Windows/Qt), the programmer creates widgets, and the library retains them in memory, handling events via callbacks.

In IMGUI, the UI is code-driven and stateless (from the library's perspective).

```cpp
// This function runs every frame
if (button(ui, "Click Me")) {
    perform_action();
}
```

**Benefits for Systems Programming:**

1. **Sync:** There is no state synchronization problem. The UI *is* the logic. If the boolean `show_window` is false, the code path that draws the window is never executed.
2. **Refactoring:** Moving a button is as simple as moving the line of code. There is no object hierarchy to restructure.
3. **Memory:** It requires almost no memory allocation for the UI structure itself, fitting perfectly with the Arena allocator model.<sup>27</sup>

### **7.3 Hot Reloading and the "Game as a Service"**

The architecture of *Handmade Hero* treats the game logic as a "service" provided to the platform layer.

* **Memory Partitioning:** The platform layer allocates a single contiguous block of memory (e.g., 1GB) via VirtualAlloc.
* **Passing State:** This block is cast to a `GameMemory` struct and passed to the game DLL.
* **Reloading:** When the game.dll file changes (detected via file timestamps), the platform layer unloads the old DLL, copies the new one to a temp file, loads it, and calls `game_get_sound_samples` or `game_update_and_render` again, passing the *same* memory block.
* **Result:** The game logic updates live without losing the state of the world (player position, inventory, etc.), because the state lives in the platform layer's memory, not the DLL's global variables.<sup>29</sup>

### **7.4 The "Result" Pattern for Error Handling**

Without exceptions, error handling must be explicit. The style uses a variation of the "Result" pattern, but often simplified to C structures.

**Pattern:**

```cpp
struct FileReadResult {
    void *content;
    u32 size;
    bool32 success;
};

FileReadResult read_file(char *filename) {
    FileReadResult result = {};
    //... Implementation...
    if (failed) { result.success = false; return result; }
    result.success = true;
    return result;
}
```

**Implication:**

Every fallible operation forces the caller to acknowledge the failure possibility. Unlike exceptions, which can be ignored until they crash the program or are caught by a catch-all handler, the return value must be handled or explicitly ignored at the call site. This leads to more robust code in systems where reliability is paramount.<sup>30</sup>

---

## **8. Community Critique and Counter-Arguments**

It is important to acknowledge the controversy surrounding this style. The "Clean Code" vs. "Performance" debate is a significant discourse in the C++ community.

### **8.1 The "Premature Optimization" Argument**

Critics often cite Knuth's "Premature optimization is the root of all evil" to argue against Muratori's style. They claim that worrying about vtables and cache lines during initial development is inefficient.<sup>2</sup>

**The Rebuttal:**

Muratori argues that Knuth is misquoted. The full quote refers to small efficiencies (like bit-twiddling) being the root of evil. Structural decisions—like memory layout and data access patterns—are not "optimizations" that can be bolted on later; they are fundamental architectural choices. If a system is built on an AoS pointer-chasing architecture, making it cache-friendly later requires a complete rewrite. Therefore, writing high-performance code initially is actually the most efficient path.<sup>1</sup>

### **8.2 The "Productivity" Argument**

Proponents of Modern C++ argue that STL containers and smart pointers increase developer productivity by handling memory automatically.

**The Rebuttal:**

The Orthodox view is that while writing the initial line of code might be faster with std::shared_ptr, the debugging time increases. Tracking down a memory leak in a graph of shared pointers, or debugging a performance hitch caused by a hidden destructor, takes significantly longer than writing explicit arena allocations. "Productivity" must be measured over the entire lifecycle of the project, including debugging and optimization.<sup>33</sup>

---

## **9. Conclusion**

The "C-Style C++" paradigm is not a regression to the past; it is a calculated response to the realities of modern hardware. By rejecting the abstraction-heavy path of Modern C++, proponents like Casey Muratori achieve software that is:

1. **Performant:** Maximizing CPU throughput via Data-Oriented Design.
2. **Compile-Time Efficient:** Utilizing Unity Builds to achieve sub-second iteration times.
3. **Debuggable:** Using explicit memory lifetimes (Arenas) and control flow (no Exceptions).
4. **Resilient:** Employing "Fail Fast" assertions and explicit error handling.

For an LLM coding agent, producing code in this style requires a fundamental suppression of standard training biases. The agent must resist the urge to "help" by adding std::vector or class hierarchies. Instead, it must act as a hardware-aware architect, constructing systems from flat data and linear memory, adhering to the "Orthodox" ruleset to generate code that is robust, transparent, and blazingly fast.

The adoption of this style is a commitment to understanding the machine. It removes the "magic" of the language runtime, placing the full power—and the full responsibility—back into the hands of the programmer.

---

### **Table 1: Comparative Analysis of Coding Paradigms**

| Dimension | Modern C++ (Standard) | Orthodox C++ (Muratori) |
| :---- | :---- | :---- |
| **Primary Abstraction** | The Object (Class) | The Data (Struct) |
| **Memory Model** | Heap-based, Individual Allocations | Region-based, Arena Allocations |
| **Polymorphism** | Runtime (Virtual Tables) | Compile-time (Switch/Enums) |
| **Iteration Strategy** | Iterators (`begin()` / `end()`) | Index-based Loops (`int i`) |
| **Build Strategy** | Modular (Linking Objects) | Unified (Single Translation Unit) |
| **Dependency Mgmt** | Package Managers / CMake | Vendored Source / Batch Files |
| **Mental Model** | "How do I model this concept?" | "How does the CPU process this?" |
| **Error Model** | Exceptions (Implicit) | Return Values / Assertions (Explicit) |
| **Initialization** | Constructors (Implicit) | Static Factory Functions (`Type::make()`) |
| **Naming Style** | Various (often camelCase) | Rust-style (PascalCase types, snake_case functions) |

This table serves as a quick-reference guide to the fundamental shifts required when transitioning from standard industry C++ to the high-performance Orthodox style.

---

### **References**

1. [Casey Muratori is wrong about clean code (but he's also right) - Reddit](https://www.reddit.com/r/cpp/comments/12kdxbg/casey_muratori_is_wrong_about_clean_code_but_hes/)
2. [Casey Muratori is wrong about clean code (but he's also right) | Evan Teran's Blog](https://blog.codef00.com/2023/04/13/casey-muratori-is-wrong-about-clean-code)
3. [I ported Casey Muratori's C++ example of "clean code" to Rust, here what I found - Reddit](https://www.reddit.com/r/rust/comments/11fkfib/i_ported_casey_muratoris_c_example_of_clean_code/)
4. [What is your opinion on Orthodox C++ ? : r/cpp - Reddit](https://www.reddit.com/r/cpp/comments/195s9lr/what_is_your_opinion_on_orthodox_c/)
5. [Orthodox C++, a Good or Bad Idea?](https://a4z.gitlab.io/blog/2024/01/14/OrthodoxCpp.html)
6. [Orthodox C++ · GitHub](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b)
7. [Semantic Compression - Casey Muratori](https://caseymuratori.com/blog_0015)
8. [Object-oriented smells - GitHub Gist](https://gist.github.com/PoignardAzur/a442226d2ea2ddc56b70dd8bdf845458)
9. [Programming as semantic compression - Reddit](https://www.reddit.com/r/programming/comments/5lt06f/programming_as_semantic_compression/)
10. [Complexity and Granularity - Casey Muratori](https://caseymuratori.com/blog_0016)
11. ["Compression-oriented programming" - Casey Muratori's blog - Reddit](https://www.reddit.com/r/programming/comments/26p35l/compressionoriented_programming_casey_muratoris/)
12. [What do you think of Casey Muratori's memory allocation method? : r/gamedev - Reddit](https://www.reddit.com/r/gamedev/comments/1eaf5wi/what_do_you_think_of_casey_muratoris_memory/)
13. [RAII and batch allocation : r/cpp_questions - Reddit](https://www.reddit.com/r/cpp_questions/comments/1p3oxk1/raii_and_batch_allocation/)
14. [My comment for too many years is that C/C++ fails to deal with three issues - Hacker News](https://news.ycombinator.com/item?id=26938245)
15. [Memory Arenas vs. Casting Memory - Handmade Hero](https://hero.handmade.network/forums/code-discussion/t/3261-memory_arenas_vs._casting_memory)
16. [FSM-Visualizer/code/memory_arena.hpp at master - GitHub](https://github.com/chr-1x/FSM-Visualizer/blob/master/code/memory_arena.hpp)
17. [Preallocted memory and dynamic arrays - Handmade Hero](https://hero.handmade.network/forums/code-discussion/t/2571-preallocted_memory_and_dynamic_arrays)
18. [Speaking of "Build" -> Performance / Details? - Handmade Hero](https://hero.handmade.network/forums/code-discussion/t/485-speaking_of_build_-_performance___details)
19. [File organization & unity build - Handmade Hero](https://hero.handmade.network/forums/code-discussion/t/263-file_organization__unity_build)
20. [Day 1. Setting Up the Windows Build - GitHub Pages](https://yakvi.github.io/handmade-hero-notes/html/day1.html)
21. [Code and study notes of Casey Muratori's awesome Handmade Hero - GitHub](https://github.com/cj1128/handmade-hero)
22. [Thoughts on build systems? : r/cpp - Reddit](https://www.reddit.com/r/cpp/comments/qpd2pc/thoughts_on_build_systems/)
23. [On C++ Build Systems | Jacob Pike's Blog](https://www.jacobpike.com/blog/2016/07/29/on-c-build-systems/)
24. [A string processing rant | The ryg blog - WordPress.com](https://fgiesen.wordpress.com/2013/01/30/a-string-processing-rant/)
25. [What is a not terrible way to handle strings in C? - Handmade Hero](https://hero.handmade.network/forums/code-discussion/t/1263-what_is_a_not_terrible_way_to_handle_strings_in_c)
26. [Casey Muratori against the Standard Library - Handmade Hero](https://hero.handmade.network/forums/code-discussion/t/8381-casey_muratori_against_the_standard_library)
27. [Immediate mode (computer graphics) - Wikipedia](https://en.wikipedia.org/wiki/Immediate_mode_\(computer_graphics\))
28. [Immediate-Mode Graphical User Interfaces (2005) - Casey Muratori](https://caseymuratori.com/blog_0001)
29. [Casey Muratori: Loading Game Code Dynamically in C : r/gamedev - Reddit](https://www.reddit.com/r/gamedev/comments/mlwjcg/casey_muratori_loading_game_code_dynamically_in_c/)
30. [Ask HN: Should you use exceptions, or return error codes? - Hacker News](https://news.ycombinator.com/item?id=41358077)
31. [How do you do errors in C++? Especially with RAII : r/gameenginedevs - Reddit](https://www.reddit.com/r/gameenginedevs/comments/1g4hvz5/how_do_you_do_errors_in_c_especially_with_raii/)
32. [Casey Muratori – The Big OOPs: Anatomy of a Thirty-five-year Mistake – BSC 2025 - Reddit](https://www.reddit.com/r/programming/comments/1m2ff3j/casey_muratori_the_big_oops_anatomy_of_a/)
33. [C vs C++, Objected Oriented Programming vs Data Oriented Programming : r/gamedev - Reddit](https://www.reddit.com/r/gamedev/comments/6n1351/c_vs_c_objected_oriented_programming_vs_data/)
34. [Casey Muratori knows a *lot* about optimizing performance in game engines. He th... | Hacker News](https://news.ycombinator.com/item?id=34975683)
