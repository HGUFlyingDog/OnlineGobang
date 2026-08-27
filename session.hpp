#pragma once
#include <iostream>
#include <memory>
#include "onlineManager.hpp"
#include <mutex>
#include <unordered_map>

enum sessionStatus
{
    UnLogin, // 1. 没有登录。
    Login    // 2. 已经登录。
};

class session
{
public:
    session(uint64_t SessionID)
        : m_nSessionID(SessionID), m_eSessionStatus(sessionStatus::UnLogin)
    {
        INFO_LOG("session创建成功 sessionID: %lu", SessionID);
    }
    ~session()
    {
        INFO_LOG("sessionc销毁成功 sessionID: %lu", m_nSessionID);
    }

    void setUser();
    uint64_t getUser();
    bool isLogin();

    // 设置这个 session 的状态 由 sessionManager 进行调用
    void setStatus(sessionStatus status)
    {
        m_eSessionStatus = status;
    }

    void setTimer(const webSocketServer::timer_ptr &ptrTimer)
    {
        m_ptrTimer = ptrTimer;
    }

    webSocketServer::timer_ptr getTimer()
    {
        return m_ptrTimer;
    }

private:
    uint64_t m_nSessionID;                 // 这个Session的ID
    uint64_t m_nUserID;                    // 这个 Session 是归属于哪个用户的
    sessionStatus m_eSessionStatus;        // 这个 session 的状态，现在简单初始化为已经登录
    webSocketServer::timer_ptr m_ptrTimer; // session 关联的定时器 通过这个进行定时任务的取消、重置
};

using session_ptr = std::shared_ptr<session>;

class sessionManager
{
public:
    sessionManager(webSocketServer *sever)
        : m_ptrSever(sever), m_unNextSessionID(1)
    {
    }
    ~sessionManager() = default;

    void createSession()
    {
        std::lock_guard lock(m_mMutex);
        uint64_t unCurrentID = m_unNextSessionID++;
        session_ptr ptrSession = std::make_shared<session>(unCurrentID);
        ptrSession->setStatus(sessionStatus::Login);
        m_mpSession.insert({unCurrentID, ptrSession});
    }

    session_ptr getSession(uint64_t sessionID)
    {
        std::lock_guard lock(m_mMutex);
        auto it = m_mpSession.find(sessionID);
        if (it == m_mpSession.end())
        {
            return nullptr;
        }
        return it->second;
    }

    // void removeSession(uint64_t sessionID)
    // {
    //     session_ptr ptrSession = getSession(sessionID);
    //     if (ptrSession.get() == nullptr)
    //     {
    //         INFO_LOG("sessionManager::removeSession sessionID: %lu not exist", sessionID);
    //     }

    //     m_mpSession.erase(sessionID);
    // }

    void removeSession(uint64_t sessionID, const websocketpp::lib::error_code &ec)
    {
        if (ec.value() == websocketpp::lib::asio::error::operation_aborted)
        {
            INFO_LOG("sessionManager::removeSession 移除 sessionID: %lu 计时器被取消", sessionID);
            return;
        }

        session_ptr ptrSession = getSession(sessionID);
        if (ptrSession.get() == nullptr)
        {
            INFO_LOG("sessionManager::removeSession sessionID: %lu not exist", sessionID);
        }

        m_mpSession.erase(sessionID);
    }

    /// @brief 设置 session 的过期时间,定时删除Session
    /// @param sessionID 目标 session
    /// @param ms 过期时间 单位：毫秒 如果传入 -1 的话，表示设置为永久对象
    void setSessionExpireTime(uint64_t sessionID, int ms)
    {
        // 这个依赖于 Web Socket Server 的定时器
        ///@note 在登录的时候使用的是 HTTP 短连接，此时我们创建的 session 需要在指定时间内没有通信之后进行删除。
        ///@note 如果进入到游戏大厅或者游戏房间，正在开始游戏的时候，这个 session 就应该永久存在。
        ///@note 等到退出游戏大厅或者房间的时候，这个 session 应该被重新设置为临时的 session，在长时间无通信的时候应该被删除。

        /*
        1. 对于永久的 session 对象，设置一个过期时间
        2. 对于一个临时的session对象，把它设置为永久的session对象
        3. 重置临时 session 对象的计时器
        */

        session_ptr ptrSession = getSession(sessionID);

        if (!ptrSession)
            return;

        if (ptrSession->getTimer().get() == nullptr && ms == -1) // 对于永久对象，并且要设置为永久对象的条件，直接跳过
            return;
        else if (ptrSession->getTimer().get() == nullptr)
        {
            webSocketServer::timer_ptr ptrTimer = m_ptrSever->set_timer(ms, std::bind(&sessionManager::removeSession, this, sessionID, std::placeholders::_1));
        }
        else if (ptrSession->getTimer().get() != nullptr && ms == -1) // 对于一个临时对象，把它设置为永久对象的情况
        {
            auto ptrTimer = ptrSession->getTimer();

            ///@warning 取消计时器之后 过一些时间会自动执行绑定的函数 也就是删除这里面的对象 所以要再次设置一下
            ///@note 这里面有一个重要的参数是错误码 如果错误码为asio::error::operation_aborted 表示这个操作是被取消的，我们可以不执行这个操作。
            ptrTimer->cancel();

            // ptrTimer = m_ptrSever->set_timer(0, [this, sessionID, ptrSession](const websocketpp::lib::error_code &ec)
            //                                  { this->m_mpSession.insert({sessionID, ptrSession}); });

            // ptrSession->setTimer()
        }
        else
        {
            // auto ptrTimer = ptrSession->getTimer();
            // ptrTimer->cancel();
            // auto ptrNewTimer = m_ptrSever->set_timer(ms, std::bind(&sessionManager::removeSession, this, sessionID, std::placeholders::_1));
            // ptrSession->setTimer(ptrNewTimer);

            ///@note 有一个更简洁的方法是timer.expires_from_now(std::chrono::seconds(ms)); 这个函数可以重置定时器的计时
            auto ptrTimer = ptrSession->getTimer();
            ptrTimer->expires_from_now(std::chrono::microseconds(ms));
        }

        // 判断是不是永久存在的就是看这个 session 有没有关联对应的定时器对象 如果关联定时器对象的话，那么说明它现在是一个临时的Session对象
    }

private:
    uint64_t m_unNextSessionID;
    std::mutex m_mMutex;
    std::unordered_map<uint64_t, session_ptr> m_mpSession;
    webSocketServer *m_ptrSever;
};