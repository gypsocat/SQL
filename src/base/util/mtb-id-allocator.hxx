#ifndef __MTB_UTIL_ID_ALLOCATOR_H__
#define __MTB_UTIL_ID_ALLOCATOR_H__

#include "../mtb-object.hxx"
#include "../mtb-stl-accel.hxx"
#include <deque>
#include <functional>

namespace MTB {
    /** @class IDAllocator
     * @brief 整数ID分配器, 以O(1)复杂度分配与归还ID. 会优先分配已经归还的ID.
     *        实际上是共用一个deque空间的两个链栈。
     *        ID会从0开始，恒为正。
     * @warning 该ID分配器默认线程不安全. 如果有多线程需要的话，请自备锁。
     * @warning 该ID分配器会复用已经归还的ID. 如果有删除再恢复的需要，请不要
     *          直接使用. */
    class IDAllocator: public Object {
    public:
        friend struct Iterator;
        struct Iterator {
            IDAllocator const &instance;
            int current_id;
            bool operator!=(Iterator const &it) const noexcept {
                return (&instance != &it.instance) ||
                       (current_id != it.current_id);
            }
            Iterator &operator++() {
                if (current_id != -1)
                    current_id = instance._entry_list[current_id].next;
                return *this;
            }
            int operator*() const noexcept { return current_id - 2; }
        }; // struct Iterator
        using ItemTraverseFunc = std::function<void(int)>;
    public:
        /** @fn IDAllocator()
         * @brief 构造一个空分配器 */
        IDAllocator();
        /** @fn IDAllocator(std::initializer_list<bool>) 
         * @brief 使用大括号初始化一个分配器 */
        IDAllocator(std::initializer_list<bool> allocated_list);
        /** @fn IDAllocator(bool[], int)
         * @brief 使用数组初始化一个分配器。不使用`vector<bool>`是怕出问题。 */
        IDAllocator(bool8vec const &allocated_list);
        
        /** @fn allocate()
         * @brief 在给定的AllocList对象中分配一个新的ID，并将其添加到已分配的元素链表中。
         * @return 返回分配的ID */
        int  allocate();
        /** @fn free(int)
         * @brief 释放已分配的ID, 并将该ID分配到未分配的链表中。 */
        void free(int id);
        /** @fn isAllocated(int)
         * @brief 求id是否已被分配。*/
        bool isAllocated(int id);

        /** @fn traverseAllocated(void(int))
         * @brief 按从近到远的顺序遍历所有已经分配的id */
        void traverseAllocated(ItemTraverseFunc fn);

        /** @fn traverseUnallocated(void(int))
         * @brief 按从近到远的顺序遍历所有没有分配的id */
        void traverseUnallocated(ItemTraverseFunc fn);

        /** foreach function */
        Iterator begin() const {
            return {*this, _entry_list[1].next};
        }
        Iterator end() const {
            return {*this, -1};
        }
    private:
        /** @struct Entry
         *  @brief  ID索引 */
        struct Entry {
            int32_t prev, next;
            bool    allocated;
        }; // struct Entry
        using EntryListT = std::deque<Entry>;
    private:
        EntryListT _entry_list;
        int32_t    _cur_max_id;
    }; // class IDAllocator
} // namespace MTB

#endif
