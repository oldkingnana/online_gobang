#pragma once

#include <list>
#include <condition_variable>
#include <jsoncpp/json/json.h>
#include <thread>

#include "myeasylog.hpp"
#include "common.hpp"
#include "user_table.hpp"
#include "online_manager.hpp"
#include "room_manager.hpp"

namespace oldking 
{
	// 这里没有设计match_queue这类东西,因为会有一个隐藏的问题,如果设计了matche_queue,且包含锁,那么一个matcher就会有三把锁(三个段位,三个匹配队列)
	// 同时,matcher对于wait()的调用会非常麻烦,wait()需要用到condition_variable
	// 所以matcher必须自己持有锁,也就是唤醒铃只有一个,且只能由生产者按,只有一个消费者(类似于一个守护者线程,专门处理match_loop)会醒来
	// 而且我们还要防止虚假唤醒问题,还要保证操作尽可能原子,那么解决方案就只能不设计match_queue或者采用几乎不封装的match_queue,仅服务于方便调用函数

	// matcher 是大厅匹配组件,负责把“开始匹配”的用户按积分段放入队列,并由内部匹配线程消费队列创建房间。
	// 它本质上是一个生产者/消费者模型:WebSocket 大厅业务调用 push 作为生产者投递 uid,match_loop 作为唯一消费者等待条件变量唤醒。
	// 为了避免多个队列对象分别持锁造成复杂锁顺序,三个积分段队列都由 matcher 自己统一持有和保护。
	// 它不负责维护用户连接,而是通过 online_manager 检查用户是否仍在大厅;也不负责房间细节,只在匹配成功后调用 room_manager::create_room。
	class matcher
	{
	private:
		std::list<uid_t> level_1_queue_; // point < 1000
		std::list<uid_t> level_2_queue_; // 1000 <= point < 2000
		std::list<uid_t> level_3_queue_; // 2000 <= point 
		oldking::mymutex mtx_;

		std::condition_variable_any cond_; // 下策

		oldking::user_table* user_table_p_;	
		oldking::online_manager* online_manager_p_;	
		oldking::room_manager* room_manager_p_;

		bool stop_match_; 
        std::thread match_thread_;

	public:
		// 创建匹配器并启动内部匹配线程,线程会等待队列人数足够后自动开房。
		matcher(user_table* utp, online_manager* omp, room_manager* rmp)
		: user_table_p_(utp)
		, online_manager_p_(omp)
		, room_manager_p_(rmp)
		, stop_match_(false)
		{
			match_thread_ = std::thread(&matcher::match_loop, this);	
		}

		// 销毁匹配器时停止匹配线程,唤醒等待中的线程并 join,避免后台线程悬挂。
		~matcher()
		{
			{
                oldking::lock_guard lock(mtx_);
                stop_match_ = true;
            }
            cond_.notify_all(); // 必须唤醒b,并释放线程b
            if (match_thread_.joinable()) {
                match_thread_.join();
            }
		}

		// 线程a,执行完push会去执行其他业务
		// 将用户加入对应积分段匹配队列,并唤醒匹配线程尝试配对。
		bool push(uid_t uid)
		{
			// uid合法性判断 todo 
			if (!online_manager_p_->in_hall(uid)) 
				return false;

			// 分数判断
			Json::Value slct_result;
			
			user_table_p_->slct_by_uid(uid, slct_result);
			int point = slct_result["point"].asInt();
			
			{
				oldking::lock_guard lock(mtx_);
				if(point < 1000)
					level_1_queue_.push_back(uid);
				else if(1000 <= point && point < 2000)
					level_2_queue_.push_back(uid);
				else // 2000 <= point 
					level_3_queue_.push_back(uid);
			}
		
			// 唤醒线程b
			cond_.notify_all(); // 全部唤醒合适吗?
			
			return true;
		}

		// 从对应积分段匹配队列移除用户,用于取消匹配、离开大厅或连接断开。
		void rm(uid_t uid)
		{
			Json::Value slct_result;
			
			oldking::lock_guard lock(mtx_);
		
			// 分数判断
			user_table_p_->slct_by_uid(uid, slct_result);
			int point = slct_result["point"].asInt();

			if(point < 1000)
				level_1_queue_.remove(uid);
			else if(1000 <= point && point < 2000)
				level_2_queue_.remove(uid);
			else // 2000 <= point 
				level_3_queue_.remove(uid);
		}

	private:

		// 线程b,会一直等待铃声,并重复循环
		// 匹配线程主循环:等待任一积分段队列达到两人,然后依次处理各队列。
		void match_loop()
		{
			while(true)
			{
				std::unique_lock<oldking::mymutex> lock(mtx_);

				// 阻塞(睡觉) -> 加锁 -> if 个数不够 -> 解锁 -> 睡觉
				cond_.wait(lock, 
						[this]
						() ->bool 
						{
							return 	stop_match_ || 
									level_1_queue_.size() >= 2 || 
								   	level_2_queue_.size() >= 2 || 
								   	level_3_queue_.size() >= 2;
						});

				if(stop_match_)
					break;

				handle_one_queue(level_1_queue_);
                handle_one_queue(level_2_queue_);
                handle_one_queue(level_3_queue_);
			}
		}
	
		// 消费一个积分段队列,每次取出两个仍在大厅的用户并调用 room_manager 创建房间。
		void handle_one_queue(std::list<uid_t>& q)
		{
			// 一次唤醒,多倍享受
			while (q.size() >= 2) 
			{
                uid_t uid1 = q.front(); 
				q.pop_front();
                uid_t uid2 = q.front(); 
				q.pop_front();

                // 玩家在排队期间突然掉线
                if (!online_manager_p_->in_hall(uid1)) 
				{
                    q.push_front(uid2);
                    continue; // 重新判定
                }
                if (!online_manager_p_->in_hall(uid2)) 
				{
                    q.push_front(uid1); 
                    continue;
                }

                // 开房
                room_manager_p_->create_room(uid1, uid2);
            }
		}

	};
}

