#pragma once

// Not working for MSC at the moment
#ifdef SCOPE_USE_SC22WG21
#	include "scope_exit.h"
#else
	
#include <type_traits>
#include <iostream>
#include <ostream>
#include <utility>
#include <string>

//// Alternative implementations

// ____________________________________[ uncaught_exception_count.hpp ]____________________________________
//             Copyright Evgeny Panasyuk 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER) || defined(__GNUG__) || defined(__CLANG__)
#define BOOST_UNCAUGHT_EXCEPTION_COUNT_SUPPORTED 1
#endif

namespace boost
{
	namespace exception_detail
	{
		template<typename To> inline
			To *unrelated_pointer_cast(void *from)
		{
			return static_cast<To*>(from);
		}
	}

	// uncaught_exception_count is a function similar to std::uncaught_exception from standard library,
	// but instead of boolean result it returns unsigned int showing current count of uncaught exceptions.

#if    (defined(_MSC_VER)  && _MSC_VER >= 1900) \
	|| (defined(__clang__) && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 7))) \
	|| (defined(__GNUC__)  && __GNUC__ >= 6)

	inline unsigned uncaught_exception_count()
	{
		return std::uncaught_exceptions();
	}
#else

#if defined(_MSC_VER)
	namespace exception_detail
	{
		extern "C" char * __cdecl _getptd();
	}
	inline unsigned uncaught_exception_count()
	{
		// MSVC specific. Tested on {MSVC2005SP1,MSVC2008SP1,MSVC2010SP1,MSVC2012}x{x32,x64}.
		return *exception_detail::unrelated_pointer_cast<unsigned>
			(
				exception_detail::_getptd() + (sizeof(void*) == 8 ? 0x100 : 0x90)
				);
	}
#elif defined(__GNUG__) || defined(__CLANG__)

	namespace exception_detail
	{
		extern "C" char * __cxa_get_globals();
	}
	inline unsigned uncaught_exception_count()
	{
		// Tested on {Clang 3.2,GCC 3.4.6,GCC 4.1.2,GCC 4.4.6,GCC 4.4.7}x{x32,x64}
		return *exception_detail::unrelated_pointer_cast<unsigned>
		(
				exception_detail::__cxa_get_globals() + (sizeof(void*) == 8 ? 0x8 : 0x4)
		);
	}
#endif
#endif 

	// Within one scope uncaught_exception_count can be changed only by +1.
	// uncaught_exception_count_latch is primitive which helps to determine such transition.
	//     Internally it stores and compares only last bit of uncaught_exception_count value
#ifdef BOOST_UNCAUGHT_EXCEPTION_COUNT_SUPPORTED
	class uncaught_exception_count_latch
	{
		unsigned char enter_state;
	public:
		uncaught_exception_count_latch()
			: enter_state(static_cast<unsigned char>(uncaught_exception_count() & 1))
		{
		}
		bool transitioned() const
		{
			return enter_state != (uncaught_exception_count() & 1);
		}
	};
#endif

}

enum Scope { scope_exit, scope_failure, scope_success };

namespace scope_guard
{
	class NonCopyableNonMovable
	{
		NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
		NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
		NonCopyableNonMovable &operator=(const NonCopyableNonMovable&) = delete;
		NonCopyableNonMovable &operator=(NonCopyableNonMovable&&) = delete;
	public:
		NonCopyableNonMovable() = default;
	};

	template<Scope scope_type, typename F>
	class Guard : NonCopyableNonMovable
	{
		boost::uncaught_exception_count_latch latch;
		F f;
	public:
		Guard(F &&f)
			: f(std::move(f))
		{}
		~Guard() noexcept(noexcept(f()))
		{
			if (latch.transitioned() != (scope_type == scope_success))
				f();
		}
	};

	template<typename F>
	class Guard<scope_exit, F> : NonCopyableNonMovable
	{
		F f;
	public:
		Guard(F &&f)
			: f(std::move(f))
		{}
		~Guard() noexcept(noexcept(f()))
		{
			f();
		}
	};

	// http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
	template<Scope scope_type>
	struct Aux
	{
		template<typename F>
		Guard<scope_type, typename std::decay<F>::type>
			operator*(F &&f) const
		{
			return{ std::forward<F>(f) };
		}
	};
}

#define SCOPE_CONCAT(x, y) SCOPE_CONCA2(x, y)
#define SCOPE_CONCA2(x, y) x ## y

#define scope(scope_type) auto &&SCOPE_CONCAT(aux, __LINE__) = scope_guard::Aux<scope_##scope_type>()*[&]

#endif // SCOPE_USE_SC22WG21