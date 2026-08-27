/*
在线用户管理模块 对游戏大厅和游戏房间进行管理
只有一个用户登录之后才会进行一个websocket的长连接 然后我们把连接和用户进行管理 通过用户的 ID 可以找到和他对应的连接，从而对这个用户的信息进行广播。
可以通过用户的 ID 找到用户的连接，然后实现相对应的连接中推送一些信息。
判断用户的在线情况
*/
#pragma once
#include "util.hpp"

#include <unordered_map>
#include <mutex>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> webSocketServer;

class online_manager
{
public:
    // webSocket 连接建立的时候调用
    void enterGameHall(uint64_t uid, webSocketServer::connection_ptr connect)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_mHallUser.insert(std::make_pair(uid, connect));
    }
    void enterGameRoom(uint64_t uid, webSocketServer::connection_ptr connect)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_mRoomUser.insert(std::make_pair(uid, connect));
    }

    // WebSocket 连接断开时调用
    void exitGameHall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_mHallUser.erase(uid);
    }
    void exitGameRoom(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_mRoomUser.erase(uid);
    }

    bool isInGameHall(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_mHallUser.find(uid);
        if (it == m_mHallUser.end()) // 没找到
        {
            return false;
        }
        return true;
    }
    bool isInGameRoom(uint64_t uid)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_mRoomUser.find(uid);
        if (it == m_mRoomUser.end()) // 没找到
        {
            return false;
        }
        return true;
    }

    webSocketServer::connection_ptr getConnectionFromHall(uint64_t uid)
    {
        if(uid == 0)
        {
            ERR_LOG("[ERR][onlineManager::getConnectionFromHall] uid is 0");
            return nullptr;
        }
        auto it = m_mHallUser.find(uid);
        if (it != m_mHallUser.end())
        {
            return it->second;
        }
        return nullptr;
    }

    webSocketServer::connection_ptr getConnectionFromRoom(uint64_t uid)
    {
        auto it = m_mRoomUser.find(uid);
        if (it != m_mRoomUser.end())
        {
            return it->second;
        }
        return nullptr;
    }

private:
    std::mutex m_mutex;

    // 键值是用户 ID，后边是大厅连接
    std::unordered_map<uint64_t, webSocketServer::connection_ptr> m_mHallUser;
    // 键值是用户 ID，后边是游戏连接
    std::unordered_map<uint64_t, webSocketServer::connection_ptr> m_mRoomUser;
};
