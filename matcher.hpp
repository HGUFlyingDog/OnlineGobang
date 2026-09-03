#pragma once
#include <list>
#include <mutex>
#include <condition_variable>
#include "onlineManager.hpp"
#include "util.hpp"
#include "db.hpp"
#include "room.hpp"
template <class T>
class matchQueue
{
public:
    matchQueue(/* args */);
    ~matchQueue();

    bool empty()
    {
        std::lock_guard<std::mutex> lg(m_mMutex);
        return m_lstUser.empty();
    }

    // 阻塞线程
    void wait()
    {
        std::unique_lock<std::mutex> lg(m_mMutex); // 条件变量不能使用 lock_guard
        m_cConditional.wait(lg, [this]()
                            { return m_lstUser.size() >= 2; });
    }

    // 入队并唤醒线程
    void push(const T &data)
    {
        std::lock_guard<std::mutex> lg(m_mMutex);
        m_lstUser.push_back(data);
        m_cConditional.notify_all();
    }

    // 出队数据
    bool popTwo(T &out1, T &out2)
    {
        std::lock_guard<std::mutex> lg(m_mMutex);
        if (m_lstUser.empty())
        {
            return false;
        }

        out1 = m_lstUser.front();
        m_lstUser.pop_front();

        out2 = m_lstUser.front();
        m_lstUser.pop_front();

        return true;
    }

    // 移除指定的数据
    void remove(T &data)
    {
        m_lstUser.remove(data);
    }

    int size()
    {
        std::lock_guard<std::mutex> lg(m_mMutex);
        return m_lstUser.size();
    }

private:
    // 用链表而不直接使用 queue，是因为有中间需要删除数据的场景
    std::list<T> m_lstUser;
    // 线程安全
    std::condition_variable m_cConditional;
    // 给消费者使用 条件变量主要用于阻塞消费者 如果队列中的元素小于两个的话就阻塞
    std::mutex m_mMutex;
};

template <class T>
matchQueue<T>::matchQueue(/* args */)
{
}

template <class T>
matchQueue<T>::~matchQueue()
{
}

class matcher
{

public:
    matcher(roomManager *ptrRoomManager, user_table *ptrUserTable, online_manager *ptrOnlineManager)
        : m_ptrRoomManager(ptrRoomManager),
          m_ptrUserTable(ptrUserTable),
          m_ptrOnlineManager(ptrOnlineManager),
          m_thNoraml(&matcher::thNormalEntry, this),
          m_thHigh(&matcher::thHighEntry, this),
          m_thSpuer(&matcher::thSuperlEntry, this)
    {
        INFO_LOG("初始化匹配器成功");
    }

    ~matcher()
    {
        m_thNoraml.join();
        m_thHigh.join();
        m_thSpuer.join();
    }
    // 添加用户到匹配队列里面
    bool addUser(uint64_t uid)
    {
        Json::Value JUserINfo;
        if (!m_ptrUserTable->selectById(uid, JUserINfo))
        {
            DBG_LOG("获取用户信息失败 uid = %lu", uid);
            return false;
        }

        int nScore = JUserINfo["score"].asInt();

        if (nScore <= 2000)
        {
            m_queNoraml.push(uid);
        }
        else if (nScore < 3000)
        {
            m_queHigh.push(uid);
        }
        else
        {
            m_queSuper.push(uid);
        }

        return true;
    }
    bool removeUser(uint64_t uid)
    {
        Json::Value JUserINfo;
        if (!m_ptrUserTable->selectById(uid, JUserINfo))
        {
            DBG_LOG("获取用户信息失败 uid = %lu", uid);
            return false;
        }

        int nScore = JUserINfo["score"].asInt();

        if (nScore <= 2000)
        {
            m_queNoraml.remove(uid);
        }
        else if (nScore < 3000)
        {
            m_queHigh.remove(uid);
        }
        else
        {
            m_queSuper.remove(uid);
        }

        return true;
    }

private:
    void handleMatch(matchQueue<uint64_t> &queue)
    {
        while (1)
        {
            // 如果队列的人数小于 2 的话 那么就阻塞
            if (queue.size() < 2)
            {
                queue.wait();
            }

            // 出队两个玩家 获得玩家的ID 和对应的连接信息
            uint64_t uid1, uid2;
            queue.popTwo(uid1, uid2);

            if (uid1 == 0)
            {
                ERR_LOG("用户状态异常 uid = %lu", uid1);
                continue;
            }
            auto ptrConnect1 = m_ptrOnlineManager->getConnectionFromHall(uid1);
            if (!ptrConnect1)
            {
                ERR_LOG("大厅找不到用户的连接信息 uid = %lu", uid1);
                continue;
            }

            auto ptrConnect2 = m_ptrOnlineManager->getConnectionFromHall(uid2);
            if (!ptrConnect1)
            {
                ERR_LOG("大厅找不到用户的连接信息 uid = %lu", uid1);
                queue.push(uid1);
                continue;
            }

            // 创建房间
            auto ptrRoom = m_ptrRoomManager->createRoom(uid1, uid2);
            if (!ptrRoom)
            {
                ERR_LOG("创建房间失败 uid1 = %lu, uid2 = %lu", uid1, uid2);
                queue.push(uid1);
                queue.push(uid2);
                continue;
            }

            // 对玩家进行响应
            Json::Value JResp;
            JResp["optype"] = "match_success";
            JResp["result"] = true;
            JResp["room_id"] = ptrRoom->getRoomID();
            JResp["white_id"] = ptrRoom->getWhiteID();
            JResp["black_id"] = ptrRoom->getBlackID();

            std::string body = Json_Util::serializeJson(JResp);
            ptrConnect1->send(body);
            ptrConnect2->send(body);
        }
    }
    void thNormalEntry() { return handleMatch(m_queNoraml); }
    void thHighEntry() { return handleMatch(m_queHigh); }
    void thSuperlEntry() { return handleMatch(m_queSuper); }

private:
    matchQueue<uint64_t> m_queNoraml;
    matchQueue<uint64_t> m_queHigh;
    matchQueue<uint64_t> m_queSuper;

    std::thread m_thNoraml;
    std::thread m_thHigh;
    std::thread m_thSpuer;

    roomManager *m_ptrRoomManager;
    user_table *m_ptrUserTable;
    online_manager *m_ptrOnlineManager;
};
