# Object Pool (C++)

Object Pool is a thread-safe implementation of Simple Segregated Storage, designed for fast allocation of objects of the same type.

## Features:

1. **Lock-free Allocation/Deallocation:** 
   - Object allocation and deallocation within existing pages are handled using a lock-free stack, ensuring high performance.
   - Page allocation requires locking.

2. **Constructor/Destructor Invocation:**
   - Each object allocation invokes its constructor, and each deallocation invokes its destructor.

3. **Alignment Handling:**  
   - Properly manages object alignment, ensuring compatibility with ARM architectures and types that require strict alignment on x86, such as `__m128`.

## Requirements:

- **C++17 or higher** is required.