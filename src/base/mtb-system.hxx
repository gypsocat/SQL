#pragma once
#ifndef __MTB_SYSTEM_H__
#define __MTB_SYSTEM_H__

#include "mtb-object.hxx"
#include "mtb-exception.hxx"
#include <cstdint>
#include <mutex>
#include <string_view>

namespace MTB {
    /** @class FileMapper abstract
     * @brief   文件映射器抽象类, 把不同操作系统平台的文件映射接口统一起来.
     * @warning 这个类不能被实例化! 你需要调用`MTB::CreateFileMapper()`函数! */
    class FileMapper: public Object {
    public:
        class Exception: public MTB::Exception {
            using MTB::Exception::Exception;
        }; // class FileMapper::Exception
    protected:
        FileMapper() = default;
        std::mutex _modify_lock;    // 修改锁, 在重映射的时候使用.

        virtual void _doResizeAppend() = 0;
    public:
        virtual ~FileMapper() = default;
        /** @fn get() abstract
         * @brief getter: 获取文件映射地址 */
        virtual pointer get() = 0;

        /** @fn get_filename() abstract
         * @brief getter: 获取被映射的文件名 */
        virtual std::string_view get_filename() = 0;

        /** @fn get_logical_block_size() abstract
         * @brief getter: 获取当前实例的块大小 */
        virtual int get_logical_block_size() = 0;

        /** @fn get_file_size() abstract
         * @brief getter: 获取文件大小 */
        virtual int get_file_size() = 0;

        /** @fn resizeAppend()
         * @brief 往文件的末尾附加一块 */
        void resizeAppend() {
            modifyLock();
            _doResizeAppend();
            modifyUnlock();
        }
        /** @fn tryResizeAppend()
         * @brief 往文件的末尾附加一块, 如果这个对象被锁定则返回false */
        bool tryResizeAppend() {
            if (tryModifyLock() == false)
                return false;
            _doResizeAppend();
            modifyUnlock();
            return true;
        }

        /** @fn GetLogicalBlockSize() static
         * @brief Global getter: 获取全局初始化的逻辑块大小（不是实例的!） */
        static uint32_t GetLogicalBlockSize();

        /** @fn SetLogicalBlockSize() static
         * @brief Global setter: 设置全局初始化的逻辑块大小（不是实例的!）
         *        逻辑块的大小必须是2的n次方. */
        static bool SetLogicalBlockSize(uint32_t block_size);

        /* 自带的锁操作函数 */
        inline void modifyLock()    { _modify_lock.lock(); }
        inline bool tryModifyLock() { return _modify_lock.try_lock(); }
        inline void modifyUnlock()  { _modify_lock.unlock(); }
    }; // abstract class FileMapper

    /** @fn CreateFileMapper(string_view)
     * @brief 根据文件名创建一个文件映射器,  */
    FileMapper* CreateFileMapper(std::string_view filename);
} // namespace MTB

#endif