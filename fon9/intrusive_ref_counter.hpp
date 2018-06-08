/// \cond fon9_DOXYGEN_ENABLE_BOOST
/*
 *          Copyright Andrey Semashev 2007 - 2013.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   intrusive_ref_counter.hpp
 * \author Andrey Semashev
 * \date   12.03.2009
 *
 * This header contains a reference counter class for \c intrusive_ptr.
 */

#ifndef BOOST_SMART_PTR_INTRUSIVE_REF_COUNTER_HPP_INCLUDED_
#define BOOST_SMART_PTR_INTRUSIVE_REF_COUNTER_HPP_INCLUDED_

// fonwin:[ from boost 1.58
#include "fon9/intrusive_ptr.hpp"
#include <atomic>

#define BOOST_DEFAULTED_FUNCTION(fn, ...)    fn __VA_ARGS__
// fonwin:][
//#include <boost/config.hpp>
//#include <boost/smart_ptr/detail/atomic_count.hpp>
// fonwin:]

#ifdef BOOST_HAS_PRAGMA_ONCE
#pragma once
#endif

#if defined(_MSC_VER)
#pragma warning(push)
// This is a bogus MSVC warning, which is flagged by friend declarations of intrusive_ptr_add_ref and intrusive_ptr_release in intrusive_ref_counter:
// 'name' : the inline specifier cannot be used when a friend declaration refers to a specialization of a function template
// Note that there is no inline specifier in the declarations.
#pragma warning(disable: 4396)
#endif

namespace fon9//boost
{

namespace sp_adl_block {

/*!
 * \brief Thread unsafe reference counter policy for \c intrusive_ref_counter
 *
 * The policy instructs the \c intrusive_ref_counter base class to implement
 * a reference counter suitable for single threaded use only. Pointers to the same
 * object with this kind of reference counter must not be used by different threads.
 */
struct thread_unsafe_counter
{
    typedef unsigned int type;

    static unsigned int load(unsigned int const& counter) BOOST_NOEXCEPT
    {
        return counter;
    }

    static unsigned int increment(unsigned int& counter) BOOST_NOEXCEPT
    {
        return counter++;
    }

    static unsigned int decrement(unsigned int& counter) BOOST_NOEXCEPT
    {
        return --counter;
    }
};

/*!
 * \brief Thread safe reference counter policy for \c intrusive_ref_counter
 *
 * The policy instructs the \c intrusive_ref_counter base class to implement
 * a thread-safe reference counter, if the target platform supports multithreading.
 */
struct thread_safe_counter
{
// fonwin:[
    //typedef boost::detail::atomic_count type;
   typedef std::atomic_int type;
// fonwin:]

    static unsigned int load(type const& counter) BOOST_NOEXCEPT
    {
        return static_cast< unsigned int >(counter.load(std::memory_order_relaxed));
    }

    static unsigned int increment(type& counter) BOOST_NOEXCEPT
    {
        //++counter; ref: http://www.boost.org/doc/libs/1_55_0/doc/html/atomic/usage_examples.html#boost_atomic.usage_examples.example_reference_counters
       return static_cast<unsigned int>(counter.fetch_add(1, std::memory_order_relaxed));
    }

    static unsigned int decrement(type& counter) BOOST_NOEXCEPT
    {
        //return static_cast< unsigned int >(static_cast< long >(--counter));
       if (auto bf = counter.fetch_sub(1, std::memory_order_release) - 1)
          return static_cast<unsigned int>(bf);
       std::atomic_thread_fence(std::memory_order_acquire);
       return 0u;
    }
};

template< typename DerivedT, typename CounterPolicyT = thread_safe_counter >
class intrusive_ref_counter;

template< typename DerivedT, typename CounterPolicyT >
unsigned int intrusive_ptr_add_ref(const intrusive_ref_counter< DerivedT, CounterPolicyT >* p) BOOST_NOEXCEPT;

template< typename DerivedT, typename CounterPolicyT >
unsigned int intrusive_ptr_release(const intrusive_ref_counter< DerivedT, CounterPolicyT >* p) BOOST_NOEXCEPT;

/*!
 * \brief A reference counter base class
 *
 * This base class can be used with user-defined classes to add support
 * for \c intrusive_ptr. The class contains a reference counter defined by the \c CounterPolicyT.
 * Upon releasing the last \c intrusive_ptr referencing the object
 * derived from the \c intrusive_ref_counter class, operator \c delete
 * is automatically called on the pointer to the object.
 *
 * The other template parameter, \c DerivedT, is the user's class that derives from \c intrusive_ref_counter.
 */
template< typename DerivedT, typename CounterPolicyT >
class intrusive_ref_counter
{
private:
    //! Reference counter type
    typedef typename CounterPolicyT::type counter_type;
    //! Reference counter
    mutable counter_type m_ref_counter;

public:
    /*!
     * Default constructor
     *
     * \post <tt>use_count() == 0</tt>
     */
    intrusive_ref_counter() BOOST_NOEXCEPT : m_ref_counter(0)
    {
    }

    /*!
     * Copy constructor
     *
     * \post <tt>use_count() == 0</tt>
     */
    intrusive_ref_counter(intrusive_ref_counter const&) BOOST_NOEXCEPT : m_ref_counter(0)
    {
    }

    /*!
     * Assignment
     *
     * \post The reference counter is not modified after assignment
     */
    intrusive_ref_counter& operator= (intrusive_ref_counter const&) BOOST_NOEXCEPT { return *this; }

    /*!
     * \return The reference counter
     */
    unsigned int use_count() const BOOST_NOEXCEPT
    {
        return CounterPolicyT::load(m_ref_counter);
    }

    /// fonwin: 增加與 shared_ptr 相容的做法.
    /// 用 this 建立 intrusive_ptr 指標.
    intrusive_ptr<DerivedT> shared_from_this() {
       return static_cast<DerivedT*>(this);
    }
    intrusive_ptr<const DerivedT> shared_from_this() const {
       return static_cast<const DerivedT*>(this);
    }

protected:
    /*!
     * Destructor
     */
    BOOST_DEFAULTED_FUNCTION(~intrusive_ref_counter(), {})

    friend unsigned int intrusive_ptr_add_ref< DerivedT, CounterPolicyT >(const intrusive_ref_counter< DerivedT, CounterPolicyT >* p) BOOST_NOEXCEPT;
    friend unsigned int intrusive_ptr_release< DerivedT, CounterPolicyT >(const intrusive_ref_counter< DerivedT, CounterPolicyT >* p) BOOST_NOEXCEPT;
};

template< typename DerivedT, typename CounterPolicyT >
inline unsigned int intrusive_ptr_add_ref(const intrusive_ref_counter< DerivedT, CounterPolicyT >* p) BOOST_NOEXCEPT
{
    return CounterPolicyT::increment(p->m_ref_counter);
}

template< typename DerivedT, typename CounterPolicyT >
inline unsigned int intrusive_ptr_release(const intrusive_ref_counter< DerivedT, CounterPolicyT >* p) BOOST_NOEXCEPT
{
   unsigned int res = CounterPolicyT::decrement(p->m_ref_counter);
   if (res == 0)
      intrusive_ptr_deleter(static_cast<const DerivedT*>(p));// delete static_cast<const DerivedT*>(p);
   return res;
}

// fonwin:[ custom deleter.
// You can write your own deleter:
// \code
//    class YourClass : public intrusive_ref_counter<YourClass> {
//    };
//    inline void intrusive_ptr_deleter(const YourClass* p) {
//       ...Do something before delete p;
//       delete p;
//       ...Do something after delete p;
//    }
// \endcode
template <class DerivedT>
inline void intrusive_ptr_deleter(const DerivedT* p) {
   delete p;
}
// fonwin:]

} // namespace sp_adl_block

using sp_adl_block::intrusive_ref_counter;
using sp_adl_block::thread_unsafe_counter;
using sp_adl_block::thread_safe_counter;

template <class ObjT>
class ObjHolder : public intrusive_ref_counter<ObjHolder<ObjT>>, public ObjT {
   ObjHolder(const ObjHolder& r) = delete;
   ObjHolder& operator=(const ObjHolder& r) = delete;
   ObjHolder(const ObjHolder&& r) = delete;
   ObjHolder& operator=(const ObjHolder&& r) = delete;
public:
   template <class... ArgsT>
   ObjHolder(ArgsT&&... args) : ObjT{std::forward<ArgsT>(args)...} {
   }
   ObjHolder() = default;
};

template <class ObjT>
using ObjHolderPtr = intrusive_ptr<ObjHolder<ObjT>>;

template <class ObjT, class... ArgsT>
ObjHolderPtr<ObjT> MakeObjHolder(ArgsT&&... args) {
   return ObjHolderPtr<ObjT>{new ObjHolder<ObjT>{std::forward<ArgsT>(args)...}};
}

} // namespace fon9//boost

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // BOOST_SMART_PTR_INTRUSIVE_REF_COUNTER_HPP_INCLUDED_
  /// \endcond
