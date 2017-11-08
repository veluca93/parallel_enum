#ifndef _CUCKOO_H
#define _CUCKOO_H
#include <vector>
#include <assert.h>
#include <immintrin.h>
#include <stdlib.h>
#include <stdint.h>

template<typename T, T missing = -1,
#ifdef __KNC__
    int bucket_size = 64/sizeof(T)
#else
    int bucket_size = 16/sizeof(T)
#endif
>
class cuckoo_hash_set {
public:
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef std::ptrdiff_t difference_type;
    typedef size_t size_type;
private:
    pointer ht;
    size_t mask;
    size_t sz;

    size_t hash_1(const value_type& k) const {
        return  k & mask;
    }

    size_t hash_2(const value_type& k) const {
        return ~k & mask;
    }

    void insert(value_type& k, value_type*& table) {
        int h1 = hash_1(k);
        for (int pos=0; pos<bucket_size; pos++)
            if (table[h1*bucket_size+pos] == missing) {
                table[h1*bucket_size+pos] = std::move(k);
                return;
            }
        int h2 = hash_2(k);
        for (int pos=0; pos<bucket_size; pos++)
            if (table[h2*bucket_size+pos] == missing) {
                table[h2*bucket_size+pos] = std::move(k);
                return;
            }
        bool use_hash_1 = true;
        for (unsigned i=0; i<mask; i++) {
            value_type cuckooed;
            size_t hash;
            if (use_hash_1) hash = hash_1(k);
            else hash = hash_2(k);
            int pos = 0;
            for (; pos<bucket_size; pos++)
                if (table[hash*bucket_size+pos] == missing)
                    break;
            if (pos == bucket_size) {
                cuckooed = std::move(table[hash*bucket_size]);
                pos = 1;
                for (; pos<bucket_size; pos++)
                    table[hash*bucket_size+pos-1] = std::move(table[hash*bucket_size+pos]);
                table[hash*bucket_size+pos-1] = std::move(k);
            } else {
                cuckooed = std::move(table[hash*bucket_size+pos]);
                table[hash*bucket_size+pos] = std::move(k);
            }
            use_hash_1 = hash == hash_2(cuckooed);
            k = std::move(cuckooed);
            if (k == missing) return;
        }
        rehash(table);
        insert(k, table);
    }

    void rehash(value_type*& table) {
        auto oldmask = mask;
        if (mask == 0) mask = 1;
        else mask = (mask<<1) | mask;
        pointer newt = 0;
        posix_memalign((void**)&newt, sizeof(T)*bucket_size, sizeof(T)*capacity());
        std::fill(newt, newt+capacity(), missing);
        for (size_t i=0; i<(oldmask+1)*bucket_size; i++)
            if (table[i] != missing)
                insert(table[i], newt);
        std::swap(table, newt);
        free(newt);
    }
public:
    class const_iterator {
    private:
        const cuckoo_hash_set& container;
        size_type offset;
    public:
        typedef cuckoo_hash_set::value_type value_type;
        typedef cuckoo_hash_set::const_reference const_reference;
        typedef cuckoo_hash_set::const_pointer const_pointer;
        typedef cuckoo_hash_set::difference_type difference_type;
        typedef std::forward_iterator_tag iterator_category;

        const_iterator(const cuckoo_hash_set& container, size_type offset_): container(container), offset(offset_) {
            while (offset != container.capacity() && container.ht[offset] == missing)
                ++offset;
        }
        bool operator==(const const_iterator& other) const {
            return &container == &other.container && offset == other.offset;
        }
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }

        const_iterator& operator++() {
            ++offset;
            while (offset != container.capacity() && container.ht[offset] == missing)
                ++offset;
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++*this;
            return tmp;
        }

        const_reference operator*() const {
            return container.ht[offset];
        };
    };

    typedef const_iterator iterator;
    friend class const_iterator;

    cuckoo_hash_set& operator=(const cuckoo_hash_set& other) = delete;

    cuckoo_hash_set(const cuckoo_hash_set& other): ht(nullptr) {
        posix_memalign((void**)&ht, sizeof(T)*bucket_size, sizeof(T)*other.capacity());
        for (uint64_t i=0; i<other.capacity(); i++)
            ht[i] = other.ht[i];
        mask = other.mask;
        sz = other.sz;
    };

    ~cuckoo_hash_set() {
        free(ht);
    }

    cuckoo_hash_set(): mask(0), sz(0) {
        posix_memalign((void**)&ht, sizeof(T)*bucket_size, sizeof(T)*bucket_size);
        std::fill(ht, ht+bucket_size, missing);
    }
    
    const_iterator begin() const {
        return const_iterator(*this, 0);
    }
    const_iterator end() const {
        return const_iterator(*this, capacity());
    }

    bool operator==(const cuckoo_hash_set<T>& oth) {
        if (oth.size() != size()) return false;
        for (const auto& x: oth)
            if (!count(x))
                return false;
        return true;
    }
    
    bool operator!=(const cuckoo_hash_set<T>& oth) {
        return !(*this == oth);
    }

    void insert(value_type k) {
        if (count(k)) {
            return;
        }
        insert(k, ht);
        sz++;
    }
    bool count(const value_type& k) const {
            int h1 = hash_1(k);
        int h2 = hash_2(k);
#ifndef __KNC__
        if (bucket_size == 4 && sizeof(T) == 4) {
            __m128i cmp = _mm_set1_epi32(k);
            __m128i b1 = _mm_load_si128((__m128i*)&ht[bucket_size*h1]);
            __m128i b2 = _mm_load_si128((__m128i*)&ht[bucket_size*h2]);
            __m128i flag = _mm_or_si128(_mm_cmpeq_epi32(cmp, b1), _mm_cmpeq_epi32(cmp, b2));
            return _mm_movemask_epi8(flag);
        }
#else
        if (bucket_size == 16 && sizeof(T) = 4) { 
            __m512i cmp = _mm512_set1_epi32(k);
            __m512i b1 = _mm512_load_epi32(&ht[bucket_size*h1]);
            __m512i b2 = _mm512_load_epi32(&ht[bucket_size*h2]);
            return _mm512_cmpeq_epi32_mask(b1, cmp) || _mm512_cmpeq_epi32_mask(b2, cmp);
        }
#endif
        bool result = false;
        for (unsigned i=0; i<bucket_size; i++)
            result |= (ht[(bucket_size*h1)|i] == k || ht[(bucket_size*h2)|i] == k);
        return result;
    }
    void reserve(size_type sz) {
        if (sz <= capacity()) return;
        mask++;
        while (mask <= sz/bucket_size) mask <<= 1;
        free(ht);
        posix_memalign((void**)&ht, sizeof(T)*bucket_size, sizeof(T)*capacity());
        std::fill(ht, ht+capacity(), missing);
        mask--;
    }
    size_type size() const {
        return sz;
    }
    size_type capacity() const {
        return (mask+1)*bucket_size;
    }
    bool empty() const {
        return sz == 0;
    }
    void erase(const value_type& k) {
        int h1 = hash_1(k);
        for (int pos=0; pos<bucket_size; pos++)
            if (ht[h1*bucket_size+pos] == k) {
                ht[h1*bucket_size+pos] = missing;
                sz--;
                return;
            }
        int h2 = hash_2(k);
        for (int pos=0; pos<bucket_size; pos++)
            if (ht[h2*bucket_size+pos] == k) {
                ht[h2*bucket_size+pos] = missing;
                sz--;
                return;
            }
    }
    void clear() {
        std::fill(ht, ht+bucket_size*capacity(), missing);
    }

    int front() const {
        return *begin();
    }
};
#endif
