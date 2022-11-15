#pragma once

#include <atomic>

#include "Types.h"

namespace Nerine
{

class IResource
{
public:
    NON_COPYABLE(IResource);
    NON_MOVEABLE(IResource);

    virtual u64 AddRef() = 0;
    virtual u64 Release() = 0;

protected:
    IResource() = default;
    virtual ~IResource() = default;
};

/*
 * Based on Com_Ptr.
 */
template <typename T> class RefCountPtr
{
public:
    using InterfaceType = T;

    RefCountPtr() noexcept : m_Ptr(nullptr)
    {
    }

    RefCountPtr(std::nullptr_t) noexcept : m_Ptr(nullptr)
    {
    }

    template <class U> RefCountPtr(U* otherPtr) noexcept : m_Ptr(otherPtr)
    {
        InternalAddRef();
    }

    RefCountPtr(const RefCountPtr& other) noexcept : m_Ptr(other.m_Ptr)
    {
        InternalAddRef();
    }

    template <class U>
    RefCountPtr(const RefCountPtr<U>& other,
                typename std::enable_if<std::is_convertible<U*, T*>::value,
                                        void*>::type* = nullptr) noexcept
        : m_Ptr(other.m_Ptr)
    {
        InternalAddRef();
    }

    RefCountPtr(RefCountPtr&& other) noexcept : m_Ptr(nullptr)
    {
        if (this != reinterpret_cast<RefCountPtr*>(&reinterpret_cast<u8&>(other)))
        {
            Swap(other);
        }
    }

    template <class U>
    RefCountPtr(RefCountPtr<U>&& other, typename std::enable_if<std::is_convertible<U*, T*>::value,
                                                                void*>::type* = nullptr) noexcept
        : m_Ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    ~RefCountPtr() noexcept
    {
        InternalRelease();
    }

    RefCountPtr& operator=(std::nullptr_t) noexcept
    {
        InternalRelease();
        return *this;
    }

    RefCountPtr& operator=(T* other) noexcept
    {
        if (m_Ptr != other)
        {
            RefCountPtr(other).Swap(*this);
        }

        return *this;
    }

    template <typename U> RefCountPtr& operator=(U* other) noexcept
    {
        RefCountPtr(other).Swap(*this);
        return *this;
    }

    RefCountPtr& operator=(const RefCountPtr& other) noexcept
    {
        if (m_Ptr != other.m_Ptr)
        {
            RefCountPtr(other).Swap(*this);
        }
        return *this;
    }

    template <class U> RefCountPtr& operator=(const RefCountPtr<U>& other) noexcept
    {
        RefCountPtr(other).Swap(*this);
        return *this;
    }

    RefCountPtr& operator=(RefCountPtr&& other) noexcept
    {
        RefCountPtr(static_cast<RefCountPtr&&>(other)).Swap(*this);
        return *this;
    }

    template <class U> RefCountPtr& operator=(RefCountPtr<U>&& other) noexcept
    {
        RefCountPtr(static_cast<RefCountPtr<U>&&>(other)).Swap(*this);
        return *this;
    }

    void Swap(RefCountPtr&& other) noexcept
    {
        T* tmp = m_Ptr;
        m_Ptr = other.m_Ptr;
        other.m_Ptr = tmp;
    }

    void Swap(RefCountPtr& other) noexcept
    {
        T* tmp = m_Ptr;
        m_Ptr = other.m_Ptr;
        other.m_Ptr = tmp;
    }

    [[nodiscard]] T* Get() const noexcept
    {
        return m_Ptr;
    }

    operator T*() const
    {
        return m_Ptr;
    }

    InterfaceType* operator->() const noexcept
    {
        return m_Ptr;
    }

    T** operator&()
    {
        return &m_Ptr;
    }

    [[nodiscard]] T* const* GetAddressOf() const noexcept
    {
        return &m_Ptr;
    }

    [[nodiscard]] T** GetAddressOf() noexcept
    {
        return &m_Ptr;
    }

    [[nodiscard]] T** ReleaseAndGetAddressOf() noexcept
    {
        InternalRelease();
        return &m_Ptr;
    }

    T* Detach() noexcept
    {
        T* ptr = m_Ptr;
        m_Ptr = nullptr;
        return ptr;
    }

    void Attach(InterfaceType* other)
    {
        if (m_Ptr)
        {
            auto ref = m_Ptr->Release();
            (void)ref;
        }

        m_Ptr = other;
    }

    static RefCountPtr<T> Create(T* other)
    {
        RefCountPtr<T> Ptr;
        Ptr.Attach(other);
        return Ptr;
    }

    u64 Reset()
    {
        return InternalRelease();
    }

    template <bool b, typename U = void> struct EnableIf
    {
    };

    template <typename U> struct EnableIf<true, U>
    {
    };

protected:
    InterfaceType* m_Ptr;
    template <class U> friend class RefCountPtr;

    void InternalAddRef() const noexcept
    {
        if (m_Ptr)
        {
            m_Ptr->AddRef();
        }
    }

    u64 InternalRelease() noexcept
    {
        u64 ref = 0;
        InterfaceType* temp = m_Ptr;

        if (temp)
        {
            m_Ptr = nullptr;
            ref = temp->Release();
        }

        return ref;
    }
};

template <class T> class RefCountResource : public T
{

public:
    virtual u64 AddRef() override
    {
        return ++m_RefCount;
    }

    virtual u64 Release() override
    {
        u64 result = --m_RefCount;
        if (result == 0)
        {
            delete this;
        }

        return result;
    }

private:
    std::atomic<u64> m_RefCount{1};
};

using ResourceHandle = RefCountPtr<IResource>;

} // namespace Nerine