#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace abouttt
{

template <typename T>
class ArrayIterator;

template <typename T>
class Array
{
public:
	using Iterator = ArrayIterator<T>;
	using ConstIterator = ArrayIterator<const T>;
	using ReverseIterator = std::reverse_iterator<Iterator>;
	using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
	static constexpr size_t INDEX_NONE = std::numeric_limits<size_t>::max();

public:
	Array() noexcept
		: Array(0)
	{
	}

	explicit Array(size_t capacity)
		: mData(capacity > 0 ? static_cast<T*>(::operator new(sizeof(T) * capacity)) : nullptr)
		, mCount(0)
		, mCapacity(capacity)
	{
	}

	Array(std::initializer_list<T> ilist)
		: Array(ilist.size())
	{
		std::uninitialized_copy(ilist.begin(), ilist.end(), mData);
		mCount = ilist.size();
	}

	Array(const Array& other)
		: Array(other.mCount)
	{
		std::uninitialized_copy(other.mData, other.mData + other.mCount, mData);
		mCount = other.mCount;
	}

	Array(Array&& other) noexcept
		: mData(std::exchange(other.mData, nullptr))
		, mCount(std::exchange(other.mCount, 0))
		, mCapacity(std::exchange(other.mCapacity, 0))
	{
	}

	~Array()
	{
		cleanup();
	}

public:
	Array& operator=(const Array& other)
	{
		if (this != &other)
		{
			Array temp(other);
			Swap(temp);
		}
		return *this;
	}

	Array& operator=(Array&& other) noexcept
	{
		if (this != &other)
		{
			cleanup();
			mData = std::exchange(other.mData, nullptr);
			mCount = std::exchange(other.mCount, 0);
			mCapacity = std::exchange(other.mCapacity, 0);
		}
		return *this;
	}

	Array& operator=(std::initializer_list<T> ilist)
	{
		Array temp(ilist);
		Swap(temp);
		return *this;
	}

	T& operator[](size_t index)
	{
		checkRange(index);
		return mData[index];
	}

	const T& operator[](size_t index) const
	{
		checkRange(index);
		return mData[index];
	}

	auto operator<=>(const Array& other) const
	{
		return std::lexicographical_compare_three_way(
			mData, mData + mCount,
			other.mData, other.mData + other.mCount
		);
	}

	bool operator==(const Array& other) const
	{
		return mCount == other.mCount && std::equal(mData, mData + mCount, other.mData);
	}

public:
	void Add(const T& value)
	{
		Emplace(value);
	}

	void Add(T&& value)
	{
		Emplace(std::move(value));
	}

	void Append(const Array& source)
	{
		Insert(mCount, source);
	}

	void Append(Array&& source)
	{
		Insert(mCount, std::move(source));
	}

	void Append(std::initializer_list<T> ilist)
	{
		Insert(mCount, ilist);
	}

	void Append(const T* ptr, size_t count)
	{
		Insert(mCount, ptr, count);
	}

	size_t Capacity() const noexcept
	{
		return mCapacity;
	}

	void Clear() noexcept
	{
		std::destroy_n(mData, mCount);
		mCount = 0;
	}

	bool Contains(const T& value) const
	{
		return Find(value) != INDEX_NONE;
	}

	template <typename Predicate>
	bool ContainsIf(Predicate pred) const
	{
		return FindIf(pred) != INDEX_NONE;
	}

	size_t Count() const noexcept
	{
		return mCount;
	}

	T* Data() noexcept
	{
		return mData;
	}

	const T* Data() const noexcept
	{
		return mData;
	}

	template <typename... Args>
	T& Emplace(Args&&... args)
	{
		size_t index = EmplaceAt(mCount, std::forward<Args>(args)...);
		return mData[index];
	}

	template <typename... Args>
	size_t EmplaceAt(size_t index, Args&&... args)
	{
		checkRange(index, true);
		ensureCapacity(mCount + 1);

		if (index < mCount)
		{
			std::uninitialized_move_n(mData + mCount - 1, 1, mData + mCount);
			std::move_backward(mData + index, mData + mCount - 1, mData + mCount);
		}
		std::construct_at(mData + index, std::forward<Args>(args)...);
		++mCount;

		return index;
	}

	size_t Find(const T& value) const
	{
		const T* it = std::find(mData, mData + mCount, value);
		return it != mData + mCount ? static_cast<size_t>(it - mData) : INDEX_NONE;
	}

	template <typename Predicate>
	size_t FindIf(Predicate pred) const
	{
		const T* it = std::find_if(mData, mData + mCount, pred);
		return it != mData + mCount ? static_cast<size_t>(it - mData) : INDEX_NONE;
	}

	size_t FindLast(const T& value) const
	{
		for (size_t i = mCount; i-- > 0; )
		{
			if (mData[i] == value)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	template <typename Predicate>
	size_t FindLastIf(Predicate pred) const
	{
		for (size_t i = mCount; i-- > 0; )
		{
			if (pred(mData[i]))
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	size_t Insert(size_t index, const T& value)
	{
		return EmplaceAt(index, value);
	}

	size_t Insert(size_t index, T&& value)
	{
		return EmplaceAt(index, std::move(value));
	}

	size_t Insert(size_t index, const Array& source)
	{
		return insertImpl(index, source.mData, source.mCount);
	}

	size_t Insert(size_t index, Array&& source)
	{
		size_t result = insertImpl(index, source.mData, source.mCount);
		source.Clear();
		return result;
	}

	size_t Insert(size_t index, std::initializer_list<T> ilist)
	{
		return insertImpl(index, ilist.begin(), ilist.size());
	}

	size_t Insert(size_t index, const T* ptr, size_t count)
	{
		return insertImpl(index, ptr, count);
	}

	bool IsEmpty() const noexcept
	{
		return mCount == 0;
	}

	bool Remove(const T& value)
	{
		size_t index = Find(value);
		if (index != INDEX_NONE)
		{
			RemoveAt(index);
			return true;
		}
		return false;
	}

	template <typename Predicate>
	size_t RemoveAll(Predicate pred)
	{
		T* newEnd = std::remove_if(mData, mData + mCount, pred);
		if (newEnd == mData + mCount)
		{
			return 0;
		}
		size_t removedCount = static_cast<size_t>((mData + mCount) - newEnd);
		std::destroy(newEnd, mData + mCount);
		mCount -= removedCount;
		return removedCount;
	}

	void RemoveAt(size_t index)
	{
		checkRange(index);
		std::destroy_at(mData + index);
		if (index < mCount - 1)
		{
			std::move(mData + index + 1, mData + mCount, mData + index);
		}
		--mCount;
	}

	void Reserve(size_t newCapacity)
	{
		if (newCapacity > mCapacity)
		{
			reallocate(newCapacity);
		}
	}

	void Resize(size_t newCount)
	{
		Resize(newCount, T());
	}

	void Resize(size_t newCount, const T& value)
	{
		if (newCount > mCount)
		{
			ensureCapacity(newCount);
			std::uninitialized_fill_n(mData + mCount, newCount - mCount, value);
		}
		else if (newCount < mCount)
		{
			std::destroy_n(mData + newCount, mCount - newCount);
		}
		mCount = newCount;
	}

	void Shrink()
	{
		if (mCapacity > mCount)
		{
			reallocate(mCount);
		}
	}

	template <typename Compare>
	void Sort(Compare comp)
	{
		std::sort(mData, mData + mCount, comp);
	}

	void Swap(Array& other) noexcept
	{
		std::swap(mData, other.mData);
		std::swap(mCount, other.mCount);
		std::swap(mCapacity, other.mCapacity);
	}

public: // Iterators for range-based loop support.
	Iterator begin() noexcept
	{
		return Iterator(mData);
	}

	ConstIterator begin() const noexcept
	{
		return ConstIterator(mData);
	}

	Iterator end() noexcept
	{
		return Iterator(mData + mCount);
	}

	ConstIterator end() const noexcept
	{
		return ConstIterator(mData + mCount);
	}

	ReverseIterator rbegin() noexcept
	{
		return ReverseIterator(end());
	}

	ConstReverseIterator rbegin() const noexcept
	{
		return ConstReverseIterator(end());
	}

	ReverseIterator rend() noexcept
	{
		return ReverseIterator(begin());
	}

	ConstReverseIterator rend() const noexcept
	{
		return ConstReverseIterator(begin());
	}

private:
	void checkRange(size_t index, bool bAllowEnd = false) const
	{
		if (index >= mCount + (bAllowEnd ? 1 : 0))
		{
			throw std::out_of_range("Array index out of range");
		}
	}

	void ensureCapacity(size_t minCapacity)
	{
		if (minCapacity > mCapacity)
		{
			size_t grow = mCapacity + (mCapacity >> 1); // Grow by 1.5x
			size_t newCapacity = std::max(minCapacity, mCapacity == 0 ? 8 : grow);
			reallocate(newCapacity);
		}
	}

	size_t insertImpl(size_t index, const T* ptr, size_t count)
	{
		checkRange(index, true);

		if (count == 0)
		{
			return index;
		}

		ensureCapacity(mCount + count);

		if (index < mCount)
		{
			std::uninitialized_move_n(mData + mCount - count, count, mData + mCount);
			std::move_backward(mData + index, mData + mCount - count, mData + mCount);
		}
		std::uninitialized_copy_n(ptr, count, mData + index);
		mCount += count;

		return index;
	}

	void reallocate(size_t newCapacity)
	{
		if (newCapacity == mCapacity)
		{
			return;
		}

		if (newCapacity == 0)
		{
			cleanup();
			return;
		}

		T* newData = static_cast<T*>(::operator new(sizeof(T) * newCapacity));
		size_t newCount = std::min(mCount, newCapacity);

		if (mData)
		{
			std::uninitialized_move_n(mData, newCount, newData);
			std::destroy_n(mData, mCount);
			::operator delete(mData);
		}

		mData = newData;
		mCount = newCount;
		mCapacity = newCapacity;
	}

	void cleanup() noexcept
	{
		if (mData)
		{
			std::destroy_n(mData, mCount);
			::operator delete(mData);
			mData = nullptr;
			mCount = 0;
			mCapacity = 0;
		}
	}

private:
	T* mData;
	size_t mCount;
	size_t mCapacity;
};

template <typename T>
class ArrayIterator
{
public:
	ArrayIterator() noexcept
		: mPtr(nullptr)
	{
	}

	explicit ArrayIterator(T* ptr) noexcept
		: mPtr(ptr)
	{
	}

	template<typename U = T, typename = std::enable_if_t<std::is_const_v<U>>>
	ArrayIterator(const ArrayIterator<std::remove_const_t<T>>& other) noexcept
		: mPtr(other.mPtr)
	{
	}

public:
	T& operator*() const noexcept
	{
		return *mPtr;
	}

	T* operator->() const noexcept
	{
		return mPtr;
	}

	T& operator[](size_t index) const noexcept
	{
		return *(mPtr + index);
	}

	ArrayIterator& operator++() noexcept
	{
		++mPtr;
		return *this;
	}

	ArrayIterator operator++(int) noexcept
	{
		ArrayIterator temp = *this;
		++mPtr;
		return temp;
	}

	ArrayIterator& operator--() noexcept
	{
		--mPtr;
		return *this;
	}

	ArrayIterator operator--(int) noexcept
	{
		ArrayIterator temp = *this;
		--mPtr;
		return temp;
	}

	ArrayIterator operator+(size_t n) const noexcept
	{
		return ArrayIterator(mPtr + n);
	}

	ArrayIterator operator-(size_t n) const noexcept
	{
		return ArrayIterator(mPtr - n);
	}

	ptrdiff_t operator-(const ArrayIterator& other) const noexcept
	{
		return mPtr - other.mPtr;
	}

	bool operator==(const ArrayIterator& other) const noexcept
	{
		return mPtr == other.mPtr;
	}

	bool operator!=(const ArrayIterator& other) const noexcept
	{
		return mPtr != other.mPtr;
	}

private:
	T* mPtr;
};

} // namespace abouttt
