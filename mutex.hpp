#pragma once
#include <mutex> 
#include <iostream>

namespace oldking
{
    // mymutex 是项目内部使用的互斥锁包装类,底层目前使用 C++11 std::mutex。
    // 它同时提供 Lock/Unlock 和 lock/unlock 两套接口,前者供自定义 lock_guard 使用,
    // 后者用于适配 std::unique_lock、std::condition_variable_any 等标准库组件。
    // 注意它不是递归锁,同一线程不能在已经持有锁时再次加同一把锁。
    class mymutex 
    {
    public:
        mymutex() = default;
        ~mymutex() = default;

        // 加锁,供项目自定义 lock_guard 调用。
        void Lock() 
        { 
            _mutex.lock(); 
        }

        // 解锁,供项目自定义 lock_guard 调用。
        void Unlock() 
        { 
            _mutex.unlock(); 
        }
        
        // 标准命名的加锁接口,供 std::unique_lock 等标准工具调用。
		void lock() 
        { 
            _mutex.lock(); 
        }

        // 标准命名的解锁接口,供 std::unique_lock 等标准工具调用。
        void unlock() 
        { 
            _mutex.unlock(); 
        }

        // 获取底层 std::mutex 引用,用于极少数需要直接访问原始锁的场景。
        std::mutex& get_raw_mutex() 
        { 
            return _mutex; 
        }

    private:
        std::mutex _mutex; // 换成C++11标准锁
    };

    // lock_guard 是项目自定义的 RAII 加锁工具,构造时加锁、析构时自动解锁。
    // 它用于保证函数中途 return 或异常退出时也能释放锁,避免忘记 Unlock 导致死锁。
    class lock_guard 
    {
    public:
        // 构造时立即锁住传入的 mymutex。
        explicit lock_guard(mymutex& mtx) : _mtx(mtx) 
        {
            _mtx.Lock();
        }

        // 析构时释放构造阶段锁住的 mymutex。
        ~lock_guard() 
        {
            _mtx.Unlock();
        }

        lock_guard(const lock_guard&) = delete;
        lock_guard& operator=(const lock_guard&) = delete;

    private:
        mymutex& _mtx;
    };
}




//namespace oldking
//{
//	class mymutex 
//	{
//	public:
//		mymutex()
//		{
//			pthread_mutex_init(&_mutex, nullptr);
//		}
//
//		void Lock()
//		{
//			int tmp_errno;
//			if((tmp_errno = pthread_mutex_lock(&_mutex)) != 0)
//			{
//				std::cerr << "pthread_mutex_lock err: " << strerror(tmp_errno) << std::endl;
//			}
//		}
//
//		void Unlock()
//		{
//			int tmp_errno;
//			if((tmp_errno = pthread_mutex_unlock(&_mutex)) != 0)
//			{
//				std::cerr << "pthread_mutex_lock err: " << strerror(tmp_errno) << std::endl;
//			}
//		}
//
//		~mymutex()
//		{
//			pthread_mutex_destroy(&_mutex);
//		}
//
//	private:
//		pthread_mutex_t _mutex;
//	};
//
//}
//
//namespace oldking
//{
//    class lock_guard 
//    {
//    public:
//        // 构造时自动加锁
//        explicit lock_guard(mymutex& mtx) : _mtx(mtx) 
//        {
//            _mtx.Lock();
//        }
//
//        // 析构时自动解锁 RAII
//        ~lock_guard() 
//        {
//            _mtx.Unlock();
//        }
//
//        // 禁用拷贝构造和赋值
//        lock_guard(const lock_guard&) = delete;
//        lock_guard& operator=(const lock_guard&) = delete;
//
//    private:
//        mymutex& _mtx;
//    };
//}
