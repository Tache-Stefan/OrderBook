#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <memory>
#include <cstdlib>

#ifdef _WIN32
#include <malloc.h>
#endif

template<typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    MemoryPool() {
        allocate_block();
    }

    ~MemoryPool() {
        for (std::byte* block : m_blocks) {
            aligned_deallocate(block);
        }
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    MemoryPool(MemoryPool&& other) noexcept
        : m_blocks(std::move(other.m_blocks)),
          m_free_list(other.m_free_list),
          m_allocated_count(other.m_allocated_count),
          m_capacity(other.m_capacity) {
        other.m_free_list = nullptr;
        other.m_allocated_count = 0;
        other.m_capacity = 0;
    }

    MemoryPool& operator=(MemoryPool&& other) noexcept {
        if (this != &other) {
            for (std::byte* block : m_blocks) {
                aligned_deallocate(block);
            }

            m_blocks = std::move(other.m_blocks);
            m_free_list = other.m_free_list;
            m_allocated_count = other.m_allocated_count;
            m_capacity = other.m_capacity;

            other.m_free_list = nullptr;
            other.m_allocated_count = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    void reserve(const size_t count) {
        while (m_capacity < count) {
            allocate_block();
        }
    }

    T* allocate() {
        if (m_free_list == nullptr) {
            allocate_block();
        }

        Node* node = m_free_list;
        m_free_list = node->next;
        ++m_allocated_count;

        return reinterpret_cast<T*>(node);
    }

    void deallocate(T* ptr) noexcept {
        if (ptr == nullptr) return;

        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = m_free_list;
        m_free_list = node;
        --m_allocated_count;
    }

    template<typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    void destroy(T* ptr) noexcept {
        if (ptr == nullptr) return;
        ptr->~T();
        deallocate(ptr);
    }

    [[nodiscard]] size_t allocated_count() const noexcept {
        return m_allocated_count;
    }
    [[nodiscard]] size_t capacity() const noexcept {
        return m_capacity;
    }

private:
    struct Node {
        Node* next;
    };

    static_assert(sizeof(T) >= sizeof(Node), "Size of T must be at least pointer-sized");

    static void* aligned_allocate(const size_t size, const size_t alignment) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
    }

    static void aligned_deallocate(void* ptr) {
#ifdef _WIN32
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    void allocate_block() {
        size_t count = BlockSize / sizeof(T);
        if (count == 0) count = 1;

        size_t alloc_size = count * sizeof(T);
        size_t alignment = alignof(T);

        void* raw_memory = aligned_allocate(alloc_size, alignment);
        if (!raw_memory) throw std::bad_alloc();

        m_blocks.push_back(static_cast<std::byte*>(raw_memory));

        std::byte* raw = static_cast<std::byte*>(raw_memory);

        for (size_t i = 0; i < count; ++i) {
            Node* node = reinterpret_cast<Node*>(raw + i * sizeof(T));
            node->next = m_free_list;
            m_free_list = node;
        }

        m_capacity += count;
    }

    std::vector<std::byte*> m_blocks;
    Node* m_free_list = nullptr;
    size_t m_allocated_count = 0;
    size_t m_capacity = 0;
};
