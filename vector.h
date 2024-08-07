#pragma once

#include <exception>
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <concepts>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

// TODO инициализации поправь в конструкторах

namespace constants {
    inline constexpr size_t realloc_factor = 2;
}

// а почему в std везде пишут constexpr
namespace container {
    template<typename Type, typename Allocator = std::allocator<Type>>
    class vector {
    public:
        using allo_tr = std::allocator_traits<Allocator>;
        using value_type = Type;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = Type*;
        using const_pointer = const pointer;
        using const_alloc_reference = const Allocator&;
        using init_list_type = std::initializer_list<Type>;
        using iterator = random_access::iterator<Type>;
        using const_iterator = random_access::iterator<const Type>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using difference_type = std::ptrdiff_t;


        constexpr vector() noexcept : m_vector{ nullptr }
        {}

        // делег конструктор как?
        constexpr explicit vector(const_alloc_reference allocator) noexcept
            : m_allocator{ allocator }, vector()
        {}

        constexpr explicit vector(size_t length, const_alloc_reference allocator = Allocator())
            : m_allocator{ allocator }
        {
            allocate_and_copy_construct(length, length);
        }

        constexpr explicit vector(size_t length, const_reference value, const_alloc_reference allocator = Allocator())
            : m_allocator{ allocator } {
            allocate_and_copy_construct(length, length, value);
        }

        constexpr explicit vector(init_list_type values, const_alloc_reference allocator = Allocator())
            : m_allocator{ allocator } {
            allocate(values.size());
            construct_init_list(values);
        }

        // TODO uniti_value
        template<typename input_iter>
        constexpr vector(input_iter first, input_iter last, const_alloc_reference allocator = Allocator()) {
            size_t size = std::distance(last, first);
            allocate(size);

            for (size_t index = 0; index < size; ++index) {
                allo_tr::construct(m_allocator, m_vector + index, *(first + index));
                ++m_size;
            }
        }

        // cpy
        // ахаххахаха это copy
        // никогда не думал что memcpy прикольно читается: "Мэм, сру"
        // TODO делег конструктор выше
        constexpr vector(const vector& other) {
            m_allocator = allo_tr::select_on_container_copy_construction(other.get_allocator());
            copy(other);
        }

        // TODO делег конструктор 2 выше
        constexpr vector(const vector& other, const_alloc_reference allocator)
            : m_allocator{ allocator } {
            copy(other);
        }

        constexpr vector& operator=(const vector& other) {
            if (this == &other)
                return *this;

            // TODO think about Copy and swap

            destruct(size());

            if (other.m_vector)
            {
                if constexpr (allo_tr::propagate_on_container_copy_assignment::value)
                    m_allocator = other.get_allocator();

                if (other.size() > capacity())
                    reallocate(capacity(), other.size());

                uninitialized_alloc_copy(other);
            }
            else {
                m_vector = nullptr;
                m_size = 0;
            }
            return *this;
        }

        constexpr vector(vector&& other) noexcept
            : m_vector{ other.m_vector }, m_size{ other.m_size }, m_capacity{ other.m_capacity }, m_allocator{ std::move(other.m_allocator) } {
            reset(other);
        }

        constexpr vector(vector&& other, const_alloc_reference allocator) noexcept
            : m_size{ other.m_size }, m_capacity{ other.m_capacity }, m_allocator{ std::move(allocator) }
        {
            // TODO check if noexcept
            // TODO use move iterators
            // TODO делег на конструктор выше с итераторами

            if (allocator != other.get_allocator())
                uninitialized_alloc_move(std::move(other));
            else {
                m_vector = other.m_vector;
                reset(other);
            }
        }

        constexpr vector& operator=(vector&& other) noexcept
        {
            if (this == &other)
                return *this;

            if constexpr (allo_tr::propagate_on_container_move_assignment::value) {
                deallocate_and_destruct(capacity(), size());
                m_allocator = other.get_allocator();
                m_vector = other.m_vector;
                reset(other);
            }
            else if (m_allocator == other.m_allocator) {
                deallocate_and_destruct(capacity(), size());
                m_vector = other.m_vector;
                reset(other);
            }
            else {
                destruct(size());
                reallocate(capacity(), other.capacity());
                uninitialized_alloc_move(std::move(other));
            }

            m_size = other.m_size;
            m_capacity = other.m_capacity;

            return *this;
        }

        ~vector() noexcept
        {
            deallocate_and_destruct(m_capacity, m_size);
        }


        constexpr vector& operator=(init_list_type values)
        {
            // TODO сначала удаляешь, а потом присваиваешь
            destruct(m_size);
            if (values.size() > capacity())
                reallocate(capacity(), values.size());

            construct_init_list(values);
            return *this;
        }

        constexpr friend bool operator==(const vector& first, const vector& second) {
            return (first.m_size == second.m_size
                && std::equal(
                    first.m_vector,
                    first.m_vector + first.size(),
                    second.m_vector,
                    second.m_vector + second.size())
            );
        }

        constexpr bool operator!= (const vector& other) const {
            return !(*this == other);
        }

        // TODO think if return equal
        constexpr bool operator< (const vector& other) const {
            return std::lexicographical_compare(m_vector, m_vector + size(), other.begin(), other.end());
        }

        constexpr bool operator> (const vector& other) const {
            return !(*this < other);
        }

        constexpr bool operator<=(const vector& other) const {
            return !(other < *this);
        }

        constexpr bool operator>=(const vector& other) const {
            return !(*this < other);
        }


        constexpr Allocator get_allocator() const noexcept
        {
            return m_allocator;
        }


        // get el
        constexpr reference at(size_t index) {
            return index < size()
                ? m_vector[index]
                : throw std::out_of_range("Index out of range");
        }

        constexpr const_reference at(size_t index) const {
            return index < size()
                ? m_vector[index]
                : throw std::out_of_range("Index out of range");
        }

        constexpr reference operator[](size_t index) {
            // почему в std есть типа такого
            // assert(index < size() && "Index out of range");

            return m_vector[index];
        }

        constexpr const_reference operator[](size_t index) const {
            // почему в std есть типа такого
            // assert(index < size() && "Index out of range");

            return m_vector[index];
        }

        constexpr pointer data() noexcept {
            return (size() != 0) ? m_vector : nullptr;
        }

        constexpr const_pointer data() const noexcept {
            return (size() != 0) ? m_vector : nullptr;
        }

        constexpr reference back() {
            return *(end() - 1);
        }

        constexpr const_reference back() const {
            return *(end() - 1);
        }

        constexpr reference front() {
            return *(begin());
        }

        constexpr const_reference front() const {
            return *(begin());
        }

        // iter
        constexpr iterator begin() noexcept {
            return m_vector;
        }

        constexpr const_iterator begin() const noexcept {
            return m_vector;
        }

        constexpr const_iterator cbegin() const noexcept {
            return m_vector;
        }

        constexpr reverse_iterator rbegin() noexcept {
            //return (m_vector + size() - 1);
            // TODO check
            return std::make_reverse_iterator(end());
        }

        constexpr const_reverse_iterator crbegin() const noexcept {
            //return (m_vector + size() - 1);
            // TODO check
            return std::make_reverse_iterator(end());
        }
        constexpr iterator end() noexcept {
            return m_vector + size();
        }
        constexpr const_iterator end() const noexcept {
            return m_vector + size();
        }
        constexpr const_iterator cend() const noexcept {
            return m_vector + size();
        }
        constexpr reverse_iterator rend() noexcept {
            return std::make_reverse_iterator(begin());
        }
        // TODO make reverse interator
        constexpr const_reverse_iterator rend() const noexcept {
            return (m_vector);
        }

        // capacity
        constexpr size_t size() const noexcept {
            return m_size;
        }

        constexpr size_t max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        constexpr size_t capacity() const noexcept {
            return m_capacity;
        }

        constexpr bool empty() const noexcept {
            return m_size == 0;
        }

        constexpr void reserve(size_t capacity) {
            if (capacity > max_size())
                throw std::length_error("Capacity allocated exceeds max_size()");
            else if (capacity > m_capacity)
                reallocate_strong_guarantee(capacity);
        }

        constexpr void shrink_to_fit() {
            if (m_capacity != m_size) {
                reallocate_strong_guarantee(m_size);
            }
        }

        constexpr void clear() noexcept {
            destruct(m_size);
        }

        constexpr iterator insert(const iterator pos, const_reference value) {
            return emplace(pos, value);
        }

        constexpr iterator insert(const iterator pos, value_type&& value) {
            return emplace(pos, std::move(value));
        }

        constexpr iterator insert(const iterator pos, size_t count, const_reference value) {
            size_t pos_index_position = std::distance(pos, begin());
            if (size() + count < capacity()) {
                if (pos == end()) {
                    insert_end_strong_guarantee(value);
                }
                else
                    shift_and_construct(pos_index_position, value, count);
            }
            else {
                do {
                    if (m_capacity == 0) m_capacity = 1;
                    m_capacity *= constants::realloc_factor;
                } while (m_capacity < m_size);

                reallocate_strong_guarantee(m_capacity);
                shift_and_construct(pos_index_position, value, count);
            }
            return count == 0 ? pos : iterator(m_vector + pos_index_position);
        }

        constexpr iterator erase(const iterator pos) {
            assert(pos <= end() && "Vector subscript out of range");
            size_t pos_index_position = std::distance(pos, begin());
            allo_tr::destroy(m_allocator, m_vector + pos_index_position);

            if constexpr (std::is_nothrow_move_constructible<Type>::value) {
                std::move(m_vector + pos_index_position + 1, m_vector + size(), m_vector + pos_index_position);
            }
            else
                std::copy(m_vector + pos_index_position + 1, m_vector + size(), m_vector + pos_index_position);

            --m_size;
            return (end() == pos) ? end() : iterator(m_vector + pos_index_position);
        }

        constexpr iterator erase(const iterator first, const iterator last) {
            bool last_equals_end = (last == end());
            assert(first <= end() && "Vector's first argument out of range");
            assert(last <= end() && "Vector's second argument out of range");
            assert(first <= last && "Vector's first argument smaller than second argument");

            size_t first_position = std::distance(first, begin());
            size_t last_position = std::distance(last, begin());
            size_t difference = last_position - first_position;

            for (size_t index = first_position; index < last_position; ++index) {
                allo_tr::destroy(m_allocator, m_vector + index);
            }
            if constexpr (std::is_nothrow_move_constructible<Type>::value) {
                std::move(m_vector + last_position, m_vector + size(), m_vector + first_position);
            }
            else
                std::copy(m_vector + last_position, m_vector + size(), m_vector + first_position);

            m_size -= difference;
            return (last_equals_end)
                ? iterator(m_vector + last_position)
                : iterator(m_vector + first_position);
        }

        constexpr void pop_back() noexcept {
            allo_tr::destroy(m_allocator, m_vector + size() - 1);
            m_size -= 1;
        }

        constexpr void resize(size_t count, const_reference value = value_type()) {
            auto temp_size = size();

            if (count < size()) {
                for (size_t index = count; index < temp_size; ++index)
                    pop_back();
            }
            else {
                if (count > capacity())
                    reallocate_strong_guarantee(count);
                for (size_t index = temp_size; index < count; ++index)
                    insert_end_strong_guarantee(value);
            }
        }

        template<typename...Args>
        constexpr iterator emplace(const iterator pos, Args&&...args) {
            assert(pos <= end() && "Vector's argument out of range");
            size_t pos_index_position = std::distance(pos, begin());

            if (size() + 1 < capacity()) {
                if (pos == end()) {
                    try {
                        allo_tr::construct(m_allocator, m_vector + size(), std::forward<Args>(args)...);
                    }
                    catch (...) {
                        allo_tr::destroy(m_allocator, m_vector + size());
                        throw;
                    }
                    ++m_size;
                }
                else {
                    shift_and_construct(pos_index_position, std::forward<Args>(args)...);
                }
            }
            else {
                do {
                    if (m_capacity == 0) m_capacity = 1;
                    m_capacity *= constants::realloc_factor;
                } while (m_capacity < 1 + size());
                reallocate_strong_guarantee(m_capacity);
                shift_and_construct(pos_index_position, std::forward<Args>(args)...);
            }

            return iterator(m_vector + pos_index_position);
        }

        template<typename...Args>
        constexpr reference emplace_back(Args&&...args) {
            // TODO для ускорения можешь скопировать кусок кода сверху
            emplace(end(), std::forward<Args>(args)...);
            return *(m_vector + size() - 1);
        }

        constexpr void push_back(const Type& value) {
            emplace_back(value);
        }

        constexpr void push_back(Type&& value) {
            emplace_back(std::move(value));
        }

        constexpr void swap(vector& other) noexcept {
            if (this == &other)
                return;

            if (allo_tr::propagate_on_container_swap::value
                || allo_tr::is_always_equal::value)
                std::swap(m_allocator, other.m_allocator);

            std::swap(m_vector, other.m_vector);
            std::swap(m_capacity, other.m_capacity);
            std::swap(m_size, other.m_size);
        }

    private:
        Type* m_vector;
        size_t m_capacity;
        size_t m_size;
        Allocator m_allocator;

        constexpr void reset(vector& other) noexcept {
            other.m_vector = nullptr;
            other.m_capacity = 0;
            other.m_size = 0;
        }

        constexpr void allocate(size_t capacity) {
            m_vector = allo_tr::allocate(m_allocator, capacity);
            m_capacity = capacity;
        }

        constexpr void deallocate(size_t capacity) {
            allo_tr::deallocate(m_allocator, m_vector, capacity);
            m_capacity = 0;
            m_size = 0;
        }

        constexpr void reallocate(size_t old_cap, size_t new_cap) {
            // TODO if dealoc выкинет ошибку, то все пропало
            deallocate(old_cap);
            allocate(new_cap);
        }

        constexpr void construct(size_t size, const Type& value) {
            for (size_t index = 0; index < size; ++index)
                allo_tr::construct(m_allocator, m_vector + index, value);
            m_size = size;
        }

        constexpr void destruct(size_t size) {
            for (size_t index = 0; index < size; ++index)
                allo_tr::destroy(m_allocator, m_vector + index);
            m_size = 0;
        }

        constexpr void allocate_and_copy_construct(size_t capacity, size_t size, const Type& value = Type()) {
            allocate(capacity);
            construct(size, value);
        }

        constexpr void construct_init_list(std::initializer_list<Type> values) {
            m_size = values.size();

            size_t index = 0;
            for (const Type& value : values)
                allo_tr::construct(m_allocator, m_vector + (index++), value);
        }

        constexpr void deallocate_and_destruct(size_t capacity, size_t size) {
            destruct(size);
            deallocate(capacity);
        }

        constexpr void deallocate_destruct_keep_size_and_capacity(size_t size, size_t capacity) {
            for (size_t index = 0; index < m_size; ++index)
                allo_tr::destroy(m_allocator, m_vector + index);

            allo_tr::deallocate(m_allocator, m_vector, m_capacity);

            m_capacity = capacity;
            m_size = size;
        }

        constexpr void uninitialized_alloc_copy(const vector& other) {
            // TODO обрабокта ошибок
            m_size = other.m_size;

            for (size_t index = 0; index < m_size; ++index)
                allo_tr::construct(m_allocator, m_vector + index, *(other.m_vector + index));
        }

        constexpr void uninitialized_alloc_move(vector&& other) noexcept {
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            for (size_t index = 0; index < m_size; ++index)
                allo_tr::construct(m_allocator, m_vector + index, std::move(*(other.m_vector + index)));
            reset(other);
        }

        constexpr void copy(const vector& other) {
            if (other.m_vector) {
                allocate(other.m_size);
                uninitialized_alloc_copy(other);
            }
            else
                m_vector = nullptr;
        }

        constexpr void reallocate_strong_guarantee(size_t capacity) {
            Type* tempVect = allo_tr::allocate(m_allocator, capacity);

            if constexpr (std::is_nothrow_move_constructible<Type>::value
                || !std::is_nothrow_move_constructible<Type>::value
                && !std::is_copy_constructible<Type>::value)
            {
                for (size_t index = 0; index < m_size; ++index)
                    allo_tr::construct(m_allocator, tempVect + index, std::move(m_vector[index]));
            }
            else {
                size_t copiesMade = 0;
                try {
                    for (size_t index = 0; index < m_size; ++index) {
                        allo_tr::construct(m_allocator, tempVect + index, *(m_vector + index));
                        ++copiesMade;
                    }
                }
                catch (...) {
                    for (size_t index = 0; index < copiesMade; ++index)
                        allo_tr::destroy(m_allocator, tempVect + index);
                    allo_tr::deallocate(m_allocator, tempVect, capacity);

                    throw;
                }
            }

            size_t temp_cap = capacity;
            deallocate_destruct_keep_size_and_capacity(m_size, temp_cap);
            m_vector = tempVect;
            tempVect = nullptr;
        }

        // TODO check
        constexpr void shift_and_construct(size_t index_pos, const Type& value, size_t count = 1) {
            // Should provide strong exception guarantee.

            size_t copies_made = 0;
            size_t copies_made1 = 0;
            size_t copies_made2 = 0;
            auto count_after_last_element = m_vector + size() + count;
            auto last_element = m_vector + size();
            auto current_pos = m_vector + index_pos;

            try {
                for (size_t index = 0; index < count; ++index) {
                    allo_tr::construct(m_allocator, m_vector + size() + index, m_vector[size() + index]);
                    ++copies_made;
                }
            }
            catch (...) {
                for (size_t index = 0; index < copies_made; ++index)
                    allo_tr::destroy(m_allocator, m_vector + size() + index);
                throw;
            }

            try {
                while (current_pos++ != m_vector + size()) {
                    *(--(count_after_last_element)) = *(--(last_element));
                    ++copies_made1;
                }
            }
            catch (...) {
                while (copies_made1 != 0) {
                    allo_tr::destroy(m_allocator, m_vector + size() + 1 + copies_made1);
                    --copies_made1;
                }
                throw;
            }

            //std::copy_backward(m_vector + index_pos, m_vector + size(), m_vector + size() + count);
            for (size_t index = 0; index < count; ++index) {
                allo_tr::destroy(m_allocator, m_vector + index_pos + index);
            }

            try {
                for (size_t index = 0; index < count; ++index) {
                    allo_tr::construct(m_allocator, m_vector + index_pos + index, value);
                    ++copies_made2;
                }
            }
            catch (...) {
                for (size_t index = 0; index < copies_made2; ++index) {
                    allo_tr::destroy(m_allocator, m_vector + size() + index);
                }
                throw;
            }

            m_size += count;
        }

        constexpr void shift_and_construct_init(size_t pos_index_position, std::initializer_list<Type> list) {
            for (size_t index = 0; auto value : list) {
                allo_tr::construct(m_allocator, m_vector + size() + index, m_vector[size() + index]);
                ++index;
            }

            std::copy_backward(m_vector + pos_index_position, m_vector + size(), m_vector + size() + list.size());

            for (size_t index = 0; auto value : list) {
                allo_tr::destroy(m_allocator, m_vector + pos_index_position + index);
                allo_tr::construct(m_allocator, m_vector + pos_index_position + index, value);
                ++index;
            }

            m_size += list.size();
        }

        constexpr void shift_and_construct(size_t index_pos, Type&& value) {
            if (std::is_nothrow_move_constructible<Type>::value || !std::is_nothrow_move_constructible<Type>::value && !std::is_copy_constructible<Type>::value) {
                auto one_after_last_element = m_vector + size() + 1;
                auto last_element = m_vector + size();
                auto current_pos = m_vector + index_pos;

                allo_tr::construct(m_allocator, m_vector + size(), std::move(*(m_vector+size())));
                std::move_backward(m_vector + index_pos, m_vector + size(), m_vector + size() + 1);
                allo_tr::construct(m_allocator, m_vector + index_pos, std::move(value));
                m_size += 1;
            }
            else shift_and_construct(index_pos, value);
        }

        constexpr void insert_end_strong_guarantee(const Type& value) {
            try {
                allo_tr::construct(m_allocator, m_vector + m_size, value);
            }
            catch (...) {
                allo_tr::destroy(m_allocator, m_vector + m_size);
                throw;
            }
            m_size += 1;
        }

        constexpr void  insert_end_strong_guarantee(Type&& value) {
            if constexpr (std::is_nothrow_move_constructible<Type>::value)
                allo_tr::construct(m_allocator, m_vector + m_size, std::move(value));
            else
                insert_end_strong_guarantee(value);
            m_size += 1;
        }
    };

    // Erase, erase_if
    template<typename Type, typename Allocator, typename Val>
    constexpr auto erase(container::vector<Type, Allocator>& vec, const Val& value) {
        auto iter = std::remove(vec.begin(), vec.end(), value);
        auto dist = std::distance(iter, vec.end());
        vec.erase(iter, vec.end());

        return dist;
    }

    template<typename Type, typename Allocator, typename Predicate>
    constexpr auto erase_if(container::vector<Type, Allocator>& vec, Predicate predicate) {
        auto iter = std::remove_if(vec.begin(), vec.end(), predicate);
        auto dist = std::distance(iter, vec.end());
        vec.erase(iter, vec.end());

        return dist;
    }
}














namespace random_access {
    template<typename Type>
    class iterator
    {
    private:
        Type* m_iterator;

    public:
        using value_type = Type;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        // TODO using iterator_concept = std::contiguous_iterator_tag;

        constexpr iterator(Type* iter = nullptr) : m_iterator{ iter } {}

        constexpr auto operator<=>(const iterator&) const = default;
        constexpr reference operator*() const noexcept {
            return *m_iterator;
        }

        constexpr pointer operator->() const noexcept {
            return m_iterator;
        }

        constexpr iterator& operator++() noexcept {
            ++m_iterator;
            return *this;
        }

        constexpr iterator operator++(int) noexcept {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        constexpr iterator& operator--() noexcept {
            --m_iterator;
            return *this;
        }

        constexpr iterator operator--(int) noexcept {
            iterator tmp(*this);
            --(*this);
            return tmp;
        }

        constexpr iterator& operator+=(const difference_type other) noexcept {
            m_iterator += other;
            return *this;
        }

        constexpr iterator& operator-=(const difference_type other) noexcept {
            m_iterator -= other;
            return *this;
        }

        friend constexpr iterator operator+(difference_type first, const iterator& other) noexcept {
            return other.m_iterator + first;
        }

        friend constexpr iterator operator+(const iterator& first, difference_type other) noexcept {
            return first.m_iterator + other;
        }

        friend constexpr iterator operator-(const iterator& first, const difference_type other) noexcept {
            return first.m_iterator - other;
        }

        constexpr difference_type operator-(const iterator& other) const noexcept {
            return std::distance(m_iterator, other.m_iterator);
        }

        constexpr reference operator[](size_t index) const {
            return m_iterator[index];
        }

        constexpr friend bool operator== (const iterator& first, const iterator& second) {
            return first.m_iterator == second.m_iterator;
        }
    };
}