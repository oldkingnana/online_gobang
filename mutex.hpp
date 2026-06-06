#pragma once
#include <mutex> 
#include <iostream>

namespace oldking
{
    class mymutex 
    {
    public:
        mymutex() = default;
        ~mymutex() = default;

        void Lock() 
        { 
            _mutex.lock(); 
        }

        void Unlock() 
        { 
            _mutex.unlock(); 
        }
        
		void lock() 
        { 
            _mutex.lock(); 
        }

        void unlock() 
        { 
            _mutex.unlock(); 
        }

        std::mutex& get_raw_mutex() 
        { 
            return _mutex; 
        }

    private:
        std::mutex _mutex; // 换成C++11标准锁
    };

    class lock_guard 
    {
    public:
        explicit lock_guard(mymutex& mtx) : _mtx(mtx) 
        {
            _mtx.Lock();
        }

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
