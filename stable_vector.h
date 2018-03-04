#pragma once

#include <type_traits>
#include <vector>
#include <initializer_list>
#include <memory>
#include <iterator>
#include <algorithm>
#include <numeric>

#include <boost/operators.hpp>
#include <boost/container/static_vector.hpp>

#define likely_false(x) __builtin_expect((x), 0)
#define likely_true(x)  __builtin_expect((x), 1)

template <typename T, std::size_t ChunkSize = 512>
struct stable_vector
{
	using value_type = T;
	using reference = value_type&;
	using const_reference = const value_type&;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	static_assert(ChunkSize % 2 == 0, "ChunkSize needs to be a multiplier of 2");

	constexpr size_type chunk_size() const noexcept { return ChunkSize; }
	constexpr size_type max_size() const noexcept   { return std::numeric_limits<size_type>::max(); }

private:
	using chunk_type = boost::container::static_vector<T, ChunkSize>;
	using storage_type = std::vector<std::unique_ptr<chunk_type>>;

	using __self = stable_vector<T, ChunkSize>;
	using __const_self = const stable_vector<T, ChunkSize>;

	template <typename Container>
	struct iterator_base
	{
		iterator_base(Container* c = nullptr, size_type i = 0) :
			m_container(c),
			m_index(i)
		{}

		iterator_base& operator+=(size_type i) { m_index += i; return *this; }
		iterator_base& operator-=(size_type i) { m_index -= i; return *this; }
		iterator_base& operator++()            { ++m_index; return *this; }
		iterator_base& operator--()            { --m_index; return *this; }

		difference_type operator-(const iterator_base& it) { assert(m_container == it.m_container); return m_index - it.m_index; }

		bool operator< (const iterator_base& it) const { assert(m_container == it.m_container); return m_index < it.m_index; }
		bool operator==(const iterator_base& it) const { return m_container == it.m_container && m_index == it.m_index; }

	 protected:
		Container* m_container;
		size_type m_index;
	};

public:
	struct const_iterator;

	struct iterator :
		public iterator_base<__self>,
		public boost::random_access_iterator_helper<iterator, value_type>
	{
		using iterator_base<__self>::iterator_base;
		friend struct const_iterator;

		reference operator*() { return (*this->m_container)[this->m_index]; }
	};

	struct const_iterator :
		public iterator_base<__const_self>,
		public boost::random_access_iterator_helper<const_iterator, const value_type>
	{
		using iterator_base<__const_self>::iterator_base;

		const_iterator(const iterator& it) :
			iterator_base<__const_self>(it.m_container, it.m_index)
		{
		}

		const_reference operator*() const { return (*this->m_container)[this->m_index]; }

		bool operator==(const const_iterator& it) const
		{
			return iterator_base<__const_self>::operator==(it);
		}

		friend bool operator==(const iterator& l, const const_iterator& r) { return r == l; }
	};

	stable_vector() = default;
	explicit stable_vector(size_type count, const T& value);
	explicit stable_vector(size_type count);

	template <typename InputIt,
			  typename = std::enable_if_t<std::is_convertible<typename std::iterator_traits<InputIt>::iterator_category, std::input_iterator_tag>::value>>
	stable_vector(InputIt first, InputIt last);

	stable_vector(std::initializer_list<T>);

	stable_vector(const stable_vector& other);
	stable_vector(stable_vector&& other);

	stable_vector& operator=(stable_vector v);

	iterator begin() { return {this, 0}; }
	iterator end()   { return {this, size()}; }

	const_iterator begin() const { return {this, 0}; }
	const_iterator end()   const { return {this, size()}; }

	const_iterator cbegin() const { return begin(); }
	const_iterator cend()   const { return end(); }

	size_type size() const;

	size_type capacity() const { return m_chunks.size() * ChunkSize; }
	bool empty() const { return m_chunks.empty(); }

	bool operator==(const __self& c) const { return size() == c.size() && std::equal(cbegin(), cend(), c.cbegin()); }

	bool operator!=(const __self& c) const { return !operator==(c); }

	void swap(__self& v) { std::swap(m_chunks, v.m_chunks); }

	friend void swap(__self& l, __self& r) { l.swap(r); }

	reference front()             { return m_chunks.front()->front(); }
	const_reference front() const { return front(); }

	reference back()             { return m_chunks.back()->back(); }
	const_reference back() const { return back(); }

private:
	void add_chunk();
	chunk_type& current_chunk();

public:
	void reserve(size_type new_capacity);

	void push_back(const T& t);
	void push_back(T&& t);

	template <typename... Args>
	void emplace_back(Args&&... args);

	reference operator[](size_type i);

	const_reference operator[](size_type i) const;

	reference at(size_type i);

	const_reference at(size_type i) const;

private:
	storage_type m_chunks;
};







template <typename T, std::size_t ChunkSize>
stable_vector<T, ChunkSize>::stable_vector(size_type count, const T& value)
{
	for (size_type i = 0; i < count; ++i)
	{
		push_back(value);
	}
}

template <typename T, std::size_t ChunkSize>
stable_vector<T, ChunkSize>::stable_vector(size_type count)
{
	for (size_type i = 0; i < count; ++i)
	{
		emplace_back();
	}
}

template <typename T, std::size_t ChunkSize>
template <typename InputIt, typename>
stable_vector<T, ChunkSize>::stable_vector(InputIt first, InputIt last)
{
	for (; first != last; ++first)
	{
		push_back(*first);
	}
}

template <typename T, std::size_t ChunkSize>
stable_vector<T, ChunkSize>::stable_vector(const stable_vector& other)
{
	for (const auto& chunk : other.m_chunks)
	{
		m_chunks.emplace_back(std::make_unique<chunk_type>(*chunk));
	}
}

template <typename T, std::size_t ChunkSize>
stable_vector<T, ChunkSize>::stable_vector(stable_vector&& other) :
	m_chunks(std::move(other.m_chunks))
{
}

template <typename T, std::size_t ChunkSize>
stable_vector<T, ChunkSize>::stable_vector(std::initializer_list<T> ilist)
{
	for (const auto& t : ilist)
	{
		push_back(t);
	}
}

template <typename T, std::size_t ChunkSize>
stable_vector<T, ChunkSize>& stable_vector<T, ChunkSize>::operator=(stable_vector v)
{
	swap(v);
	return *this;
}

template <typename T, std::size_t ChunkSize>
typename stable_vector<T, ChunkSize>::size_type stable_vector<T, ChunkSize>::size() const
{
	return std::accumulate(m_chunks.cbegin(), m_chunks.cend(), size_type{}, [](size_type s, auto& chunk_ptr)
						   {
							   return s + chunk_ptr->size();
						   });
}

template <typename T, std::size_t ChunkSize>
void stable_vector<T, ChunkSize>::add_chunk()
{
	m_chunks.emplace_back(std::make_unique<chunk_type>());
}

template <typename T, std::size_t ChunkSize>
typename stable_vector<T, ChunkSize>::chunk_type& stable_vector<T, ChunkSize>::current_chunk()
{
	if (likely_false(m_chunks.empty() || m_chunks.back()->size() == ChunkSize))
	{
		add_chunk();
	}

	return *m_chunks.back();
}

template <typename T, std::size_t ChunkSize>
void stable_vector<T, ChunkSize>::reserve(size_type new_capacity)
{
	const std::size_t initial_capacity = capacity();
	for (difference_type i = new_capacity - initial_capacity; i > 0; i -= ChunkSize)
	{
		add_chunk();
	}
}

template <typename T, std::size_t ChunkSize>
void stable_vector<T, ChunkSize>::push_back(const T& t)
{
	current_chunk().push_back(t);
}

template <typename T, std::size_t ChunkSize>
void stable_vector<T, ChunkSize>::push_back(T&& t)
{
	current_chunk().push_back(std::move(t));
}

template <typename T, std::size_t ChunkSize>
template <typename... Args>
void stable_vector<T, ChunkSize>::emplace_back(Args&&... args)
{
	current_chunk().emplace_back(std::forward<Args>(args)...);
}

template <typename T, std::size_t ChunkSize>
typename stable_vector<T, ChunkSize>::reference
stable_vector<T, ChunkSize>::operator[](size_type i)
{
	return (*m_chunks[i / ChunkSize])[i % ChunkSize];
}

template <typename T, std::size_t ChunkSize>
typename stable_vector<T, ChunkSize>::const_reference
stable_vector<T, ChunkSize>::operator[](size_type i) const
{
	return const_cast<__self&>(*this)[i];
}

template <typename T, std::size_t ChunkSize>
typename stable_vector<T, ChunkSize>::reference
stable_vector<T, ChunkSize>::at(size_type i)
{
	if (likely_false(i >= size()))
	{
		throw std::out_of_range("stable_vector::at");
	}

	return operator[](i);
}

template <typename T, std::size_t ChunkSize>
typename stable_vector<T, ChunkSize>::const_reference
stable_vector<T, ChunkSize>::at(size_type i) const
{
	return const_cast<__self&>(*this).at(i);
}

