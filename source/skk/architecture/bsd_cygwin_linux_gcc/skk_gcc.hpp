/*
  Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Tadashi Watanabe <wac@umiushi.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef SKK_GCC_HPP
#define SKK_GCC_HPP

#include "skk_gcc_precompile.h"

namespace YaSkkServ
{
/// デバッグに関連する名前空間です。
/**
 * \attention
 * DEBUG_PRINTF はマクロ YASKKSERV_DEBUG が定義されているとき、
 * DEBUG_ASSERT はマクロ YASKKSERV_DEBUG_PARANOIA が定義されているとき
 * に有効になります。
 */
namespace Debug
{
template<bool> struct CompileTimeError;
template<> struct CompileTimeError<true> {};

template<typename T> class TypeTraits
{
        typedef char Size1;
        struct Size2
        {
                char dummy[2];
        };

        struct Test
        {
                Test(const volatile void*);
        };

        static inline Size1 get(Test);
        static inline Size2 get(...);
        static inline T& makeType();

        enum
        {
                TYPE_SIZE = sizeof(get(makeType()))
        };

        template<int type_size> static inline bool is_pointer()
        {
                return (type_size == sizeof(Size1)) ? true : false;
        }

public:
        static inline bool isPointer()
        {
                return is_pointer<TYPE_SIZE>();
        }

        static inline void isPointerCompileTimeError()
        {
                CompileTimeError<TYPE_SIZE == sizeof(Size1)>();
        }
};

template<> class TypeTraits<void>
{
public:
        static inline bool isPointer()
        {
                return false;
        }
};

#ifdef YASKKSERV_DEBUG

void printf_core(const char *filename, int line, const char *p, ...);
void print_core(const char *filename, int line, const char *p);

#define DEBUG_PRINTF(p, args...) Debug::printf_core(__FILE__, __LINE__, p, ## args)
#define DEBUG_PRINT(p) Debug::print_core(__FILE__, __LINE__, p)

#else  // YASKKSERV_DEBUG

#define DEBUG_PRINTF(p, args...)
#define DEBUG_PRINT(p)

#endif  // YASKKSERV_DEBUG




#ifdef YASKKSERV_DEBUG_PARANOIA

inline void paranoia_assert_(const char *filename, int line, bool flag)
{
        if (flag == false)
        {
                DEBUG_PRINTF("%s:%d  ASSERT()\n", filename, line);
        }
        assert(flag);
}

template<typename T> inline void paranoia_assert_range_(const char *filename, int line, T scalar, T minimum, T maximum)
{
        if ((scalar < minimum) || (scalar > maximum))
        {
                DEBUG_PRINTF("%s:%d  ASSERT_RANGE()\n"
                             " scalar = %d\n"
                             "minimum = %d\n"
                             "maximum = %d\n"
                             ,
                             filename,
                             line,
                             scalar,
                             minimum,
                             maximum);
                assert(0);
        }
}

template<typename T> inline void paranoia_assert_pointer_(const char *filename, int line, T p)
{
        TypeTraits<T>::isPointerCompileTimeError();
        if (p == 0)
        {
                DEBUG_PRINTF("%s:%d  ASSERT_POINTER()\n"
                             " p = %p\n"
                             ,
                             filename,
                             line,
                             p);
                assert(0);
        }
}

template<typename T> inline void paranoia_assert_pointer_align_(const char *filename, int line, T p, int align)
{
        TypeTraits<T>::isPointerCompileTimeError();
        if (p == 0)
        {
                DEBUG_PRINTF("%s:%d  ASSERT_POINTER_ALIGN()\n"
                             " p = %p\n"
                             ,
                             filename,
                             line,
                             p);
                assert(0);
        }
        intptr_t tmp = reinterpret_cast<intptr_t>(p);
        if ((tmp % align) != 0)
        {
                DEBUG_PRINTF("%s:%d  ASSERT_POINTER_ALIGN()\n"
                             " p = %p  align = %d\n"
                             ,
                             filename,
                             line,
                             p,
                             (tmp % align));
                assert(0);
        }
}

#define DEBUG_ASSERT(expr) Debug::paranoia_assert_(__FILE__, __LINE__, expr)
#define DEBUG_ASSERT_RANGE(scalar, minimum, maximum) Debug::paranoia_assert_range_(__FILE__, __LINE__, scalar, minimum, maximum)
#define DEBUG_ASSERT_POINTER(p) Debug::paranoia_assert_pointer_(__FILE__, __LINE__, p)
#define DEBUG_ASSERT_POINTER_ALIGN(p, align) Debug::paranoia_assert_pointer_align_(__FILE__, __LINE__, p, align)

#else  // YASKKSERV_DEBUG_PARANOIA

template<typename T> inline void paranoia_assert_pointer_(T p)
{
        TypeTraits<T>::isPointerCompileTimeError();
        (void)p;        // AVOIDWARNING
}

template<typename T> inline void paranoia_assert_pointer_align_(T p, int align)
{
        TypeTraits<T>::isPointerCompileTimeError();
        (void)p;          // AVOIDWARNING
        (void)align;      // AVOIDWARNING
}

#define DEBUG_ASSERT(expr)
#define DEBUG_ASSERT_RANGE(scalar, minimum, maximum)
#define DEBUG_ASSERT_POINTER(p) Debug::paranoia_assert_pointer_(p)
#define DEBUG_ASSERT_POINTER_ALIGN(p, align) Debug::paranoia_assert_pointer_align_(p, align)

#endif  // YASKKSERV_DEBUG_PARANOIA
}
}

//
// 以下は new/delete まわりの内部デバッグ用コードです。
//
#ifdef YASKKSERV_DEBUG
#define YASKKSERV_INTERNAL_DEBUG_NEW
#endif  // YASKKSERV_DEBUG

#ifdef YASKKSERV_INTERNAL_DEBUG_NEW

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
void *operator new(size_t size);
void *operator new(size_t size, const char *filename, int line);
void operator delete(void *p);

void *operator new[](size_t size);
void *operator new[](size_t size, const char *filename, int line);
void operator delete[](void *p);
#pragma GCC diagnostic pop

#define new new(__FILE__, __LINE__)
#define delete DEBUG_PRINTF("delete:%s:%d\n", __FILE__, __LINE__), delete

#endif  // YASKKSERV_INTERNAL_DEBUG_NEW

#endif // SKK_GCC_HPP
