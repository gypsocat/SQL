#ifndef __MTB_OBJECT_H__
#define __MTB_OBJECT_H__

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

/* 一些没有任何实际效果，只有说明作用的宏 */
#define interface struct
#define imcomplete /* 抽象类 */

namespace MTB {
    /** @class Object
     * @brief Medi ToolBox的基础类, 有基于引用计数自动内存管理功能. 不使用`std::shared_ptr`
     *        的理由是, `shared_ptr`很容易出现重复析构的问题.
     * @brief 如果你喜欢的话, 可以让你的所有类都继承`Object`. 这样我写运行时泛型就方便了. */
    class Object {
    public:
        Object() : __ref_count__(0) {}
        virtual ~Object() = default;
        alignas(uint64_t) std::atomic_int __ref_count__;
    }; // class Object

    /** @class PublicExtendsObject concept
     * @brief 验证一个类是否继承自Object. */
    template<typename ObjT>
    concept PublicExtendsObject = requires(ObjT x) { x.Object::__ref_count__; };
    
    /** @struct owned
     * @brief   和`Object`搭配使用的智能指针, 可以自动管理引用计数。你可以像std::shared_ptr
     *          那样使用MTB::make_owned模板创建，也可以直接从普通指针赋值。由于引用计数就在对
     *          象本体里面，所以不用担心shared_ptr的多重释放问题。
     * @warning 和所有其他采用引用计数的智能指针一样, 滥用这种持有所有权的指针可能会导致循环引用问题!
     * @fn operator=
     * @fn operator*
     * @fn operator<=> */
    template<PublicExtendsObject ObjT>
    struct owned {
        ObjT *__ptr;
        
        /** 构造函数 -- 无参构造得null */
        owned() noexcept : __ptr(nullptr) {}
        owned(std::nullptr_t) noexcept: __ptr(nullptr){}
        /** 构造函数 -- 从实例指针构造 */
        owned(ObjT *instance) noexcept {
            __ptr = instance;
            instance->__ref_count__++;
        }
        /** 构造函数 -- 从同类型持有所有权的指针构造*/
        owned(owned const &instance) noexcept {
            __ptr = instance.__ptr;
            __ptr->__ref_count__++;
        }
        /** 构造函数 -- 移动构造 */
        owned(owned &&rrinst) noexcept {
            __ptr = rrinst.__ptr;
            rrinst.__ptr = nullptr;
        }
        /** 析构时解引用 */
        ~owned() {
            if (__ptr == nullptr)
                return;
            __ptr->__ref_count__--;
            if (__ptr->__ref_count__ == 0)
                delete __ptr;
        }

        /** 模板：从其他类型构造，会dynamic_cast */
        template<typename ObjET>
        requires std::is_base_of<ObjT, ObjET>::value
        owned(ObjET *ext) {
            __ptr = ext;
            ext->__ref_count__++;
        }
        template<typename ObjET>
        requires std::is_base_of<ObjT, ObjET>::value
        owned(owned<ObjET> &ext) {
            auto ptr = dynamic_cast<ObjT*>(ext.__ptr);
            __ptr = ptr;
            if (ptr != nullptr)
                ptr->__ref_count__++;
        }
        template<typename ObjET>
        requires std::is_base_of<ObjT, ObjET>::value
        owned(owned<ObjET> &&ext) {
            __ptr = dynamic_cast<ObjT*>(ext.__ptr);
            ext.__ptr = nullptr;
        }

        /** 与普通指针交互、取地址 */
        operator ObjT* () const& { return __ptr; }
        operator ObjT* () && = delete;
        ObjT *get() const { return __ptr; }

        ObjT *operator->() { return __ptr; }
        ObjT &operator*() & { return *__ptr; }
        ObjT &operator*() && = delete;
        owned &operator=(owned const &ptr) {
            if (__ptr != nullptr) unref();
            if (ptr.__ptr != nullptr)
                __ptr = ptr.ref();
            else
                __ptr = nullptr;
            return *this;
        }
        owned &operator=(owned &&ptr) {
            if (__ptr != nullptr) unref();
            __ptr = ptr.__ptr;
            ptr.__ptr = nullptr;
            return *this;
        }
        owned &operator=(std::nullptr_t) {
            if (__ptr != nullptr) unref();
            __ptr = nullptr;
            return *this;
        }
        template<PublicExtendsObject ObjET>
        owned &operator=(owned<ObjET> &ptr) {
            if (__ptr != nullptr) unref();
            __ptr = dynamic_cast<ObjT*>(ptr.__ptr);
            if (__ptr != nullptr)
                ref();
            return *this;
        }

        bool operator==(owned const &ptr) const {
            return (__ptr == ptr.__ptr);
        }
        bool operator==(owned &&ptr) const {
            return (__ptr == ptr.__ptr);
        }
        bool operator==(const ObjT *ptr) const {
            return (__ptr == ptr);
        }
        template<PublicExtendsObject ObjET>
        bool operator==(const ObjET *ptr) const {
            return (__ptr == dynamic_cast<Object*>(ptr));
        }

        bool operator!=(owned const &ptr) const {
            return (__ptr != ptr.__ptr);
        }
        bool operator!=(owned &&ptr) const {
            return (__ptr != ptr.__ptr);
        }
        bool operator!=(const ObjT *ptr) const {
            return (__ptr != ptr);
        }
        template<PublicExtendsObject ObjET>
        bool operator!=(const ObjET *ptr) const {
            return (__ptr != dynamic_cast<Object*>(ptr));
        }

        /** @fn ref_count() -> int
         * @brief 返回指向的对象的引用计数 */
        int ref_count() { return __ptr->Object::__ref_count__; }
        owned &ref() { __ptr->__ref_count__++; return *this; }
        void unref() {
            __ptr->__ref_count__--;
            if (__ptr->__ref_count__ == 0)
                delete __ptr;
        }
        void reset() {
            if (__ptr == nullptr)
                unref();
            __ptr = nullptr;
        }

        /** @fn operator<=>
         * @brief 懒得写==与!=了 */
        auto operator<=>(owned const &) const = default;
    }; // struct owned

    /** @struct unowned
     * @brief 没有所有权的普通指针, 使用这个模板可以提醒你不要随便乱动这玩意。*/
    template<PublicExtendsObject ObjET>
    using unowned = ObjET*;

    /** @fn make_owned
     * @brief 类似于`std::make_shared`, 这样你就可以使用`auto`来自动推导出`owned`智能指针了.
     * @warning 该函数的参数只能是继承自`Object`的类。 */
    template<PublicExtendsObject ObjT, typename... ArgT>
    inline owned<ObjT> make_owned(ArgT... args) {
        return owned<ObjT>(new ObjT(args...));
    }

    using pointer = void*;
    // C++对bool类型没有规定, 这里定1字节
    struct bool8_t {
        inline bool8_t() noexcept: value(false){}
        inline bool8_t(bool value) noexcept: value(value){}
        inline bool8_t(bool8_t const& value) noexcept: value(value.value){}
        inline bool8_t(bool8_t &&value) noexcept: value(value.value){}
        inline bool8_t(uint8_t const u8) noexcept: value(u8 != 0){}
        template<typename IntegerT>
        inline bool8_t(IntegerT const i): value(i != 0){}

        inline operator bool() const noexcept { return value; }
        inline operator uint8_t() const noexcept { return value; }
        template<typename IntegerT>
        operator IntegerT() const { return IntegerT(value); }
        bool operator==(bool const &rhs) const noexcept { return value == rhs; }
        bool operator!=(bool const &rhs) const noexcept { return value != rhs;  }
        bool operator==(bool8_t const rhs) const noexcept { return value == rhs.value; }
        bool operator!=(bool8_t const rhs) const noexcept { return value != rhs.value; }
        template<typename IntegerT>
        bool operator==(IntegerT const rhs) const noexcept { return (value != 0) == (rhs != 0); }
        template<typename IntegerT>
        bool operator!=(IntegerT const rhs) const noexcept { return (value != 0) != (rhs != 0); }
    private:
        alignas(uint8_t) uint8_t value;
    };
} // namespace MTB

namespace std {

template<typename DestT, typename SourceT>
requires std::is_base_of<DestT, SourceT>::value || std::is_base_of<SourceT, DestT>::value
MTB::owned<DestT> dynamic_pointer_cast(MTB::owned<SourceT> &&src) {
    MTB::owned<DestT> ret;
    ret.__ptr = dynamic_cast<DestT*>(src.__ptr);
    src.__ptr = nullptr;
    return ret;
}
template<typename DestT, typename SourceT>
requires std::is_base_of<DestT, SourceT>::value || std::is_base_of<SourceT, DestT>::value
MTB::owned<DestT> dynamic_pointer_cast(MTB::owned<SourceT> const& src) {
    MTB::owned<DestT> ret;
    ret.__ptr = dynamic_cast<DestT*>(src.__ptr);
    if (ret.__ptr != nullptr)
        src.__ptr->__ref_count__++;
    return ret;
}

}

#endif