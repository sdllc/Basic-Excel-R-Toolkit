/*
* Basic Excel R Toolkit (BERT)
* Copyright (C) 2014-2017 Structured Data, LLC
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*
*/

#ifndef __UTILITY_CONTAINERS_H
#define __UTILITY_CONTAINERS_H

#include <deque>
#include <string>

using namespace std;

/**
 * locked deque for threads.  we don't overload base functions; 
 * all protected functions are explicitly named.
 */
template < class T > class locked_deque : public deque < T > {
protected:
	HANDLE hmutex;

public:
	locked_deque() {
		hmutex = ::CreateMutex(NULL, FALSE, NULL);
	}

	~locked_deque() {
		::CloseHandle(hmutex);
	}

public:
	void locked_push_back(T elt) {
		lock();
		push_back(elt);
		unlock();
	}
	
	T locked_pop_front() {
		lock();
		T elt = this->operator[](0);
		pop_front();
		unlock();
		return elt;
	}

	size_t locked_size() {
		size_t count;
		lock();
		count = size();
		unlock();
		return count;
	}

	void locked_consume(deque < T > &target) {
		lock();
		target = (deque<T>)(*this);
		clear();
		unlock();
	}

public:
	inline void lock() {
		::WaitForSingleObject(hmutex, INFINITE);
	}
	inline void unlock() {
		::ReleaseMutex(hmutex);
	}

};

/**
 * utility for appending strings.  you would think that stringstreams were more efficient,
 * but it's not clear that's the case.  in any event using a wrapper class means we can 
 * switch if desired.
 */
class locked_string_buffer {
public:
	locked_string_buffer( int reserve = 4096 ) {
		str.reserve(reserve);
		hmutex = ::CreateMutex(NULL, FALSE, NULL);
	}

	~locked_string_buffer() {
		::CloseHandle(hmutex);
	}

public:
	void append(const char *s) { 
		lock();
		str.append(s);
		unlock();
	}

	void append(std::string s) { 
		lock();
		str.append(s);
		unlock();
	}

	size_t size() {
		size_t count = 0;
		lock();
		count = str.size();
		unlock();
		return count;
	}

	void clear() {
		lock();
		str.clear(); // does this impact the reserved size?
		unlock();
	}

	string take() {
		lock();
		string s(str);
		str.clear();
		unlock();
		return s;
	}

	string& take(string &s) {
		lock();
		s = str;
		str.clear();
		unlock();
		return s;
	}

public:
	inline void lock() {
		::WaitForSingleObject(hmutex, INFINITE);
	}
	inline void unlock() {
		::ReleaseMutex(hmutex);
	}

protected:
	string str;
	HANDLE hmutex;
};

#endif // #ifndef __UTILITY_CONTAINERS_H

