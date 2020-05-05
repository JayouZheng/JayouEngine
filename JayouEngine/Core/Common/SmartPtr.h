//
// SmartPtr.h
//

#pragma once

#pragma region SmartPtr

template<typename T>
class Counter;

template<typename T>
class SmartPtr
{
private:
	Counter<T> *_ptr_cnt;

public:

	SmartPtr(T *ptr)
	{
		_ptr_cnt = new Counter<T>(ptr);
	}

	SmartPtr(const SmartPtr &other)
	{
		_ptr_cnt = other._ptr_cnt;
		_ptr_cnt->_counter++;
	}

	SmartPtr &operator=(const SmartPtr &other)
	{
		if (this == &other)
			return *this;

		_ptr_cnt->_counter--;
		if (_ptr_cnt->_counter == 0)
			delete _ptr_cnt;

		_ptr_cnt = other._ptr_cnt;
		other._ptr_cnt->_counter++;

		return *this;
	}

	T &operator*()
	{
		return *(_ptr_cnt->_ptr);
	}

	T* operator->() const throw() // no exception
	{
		return _ptr_cnt->_ptr;
	}

	bool operator!=(const T* other_ptr) const
	{
		return _ptr_cnt->_ptr != other_ptr;
	}

	bool operator==(const T* other_ptr) const
	{
		return _ptr_cnt->_ptr == other_ptr;
	}

	~SmartPtr()
	{
		_ptr_cnt->_counter--;
		if (_ptr_cnt->_counter == 0)
			delete _ptr_cnt;
	}
};

template<typename T>
class Counter
{
private:
	T *_ptr;
	int _counter;

	template<typename T>
	friend class SmartPtr;

	Counter(T *ptr)
	{
		_ptr = ptr;
		_counter = 1;
	}

	~Counter()
	{
		delete _ptr;
	}
};

#pragma endregion