#pragma once
#include <jsoncpp/json/json.h>
#include <string>
#include <mysql/mysql.h>
#include <mutex>
#include "util.hpp"

class user_table
{
public:
    user_table()
        : m_mysql(MySQL_Util::getInstance())
    {
    }
    ~user_table()
    {
    }

    bool insertUser(const Json::Value &user)
    {
        std::string strName = user["username"].asString();
        std::string strPassword = user["password"].asString();

        if (strName.empty() || strPassword.empty())
        {
            std::cout << "用户名或密码为空" << std::endl;
            return false;
        }

        Json::Value userInfo;

        if (select_byUsername(strName, userInfo))
        {
            std::cout << "用户名已存在" << std::endl;
            return false;
        }

        m_mysql.executeQuery("insert into user(username,password) values('" + strName + "','" + strPassword + "')");

        return true;
    }

    /// @brief
    /// @param user
    /// @return
    bool loginUser(Json::Value &user)
    {
        std::string strName = user["username"].asString();
        std::string strPassword = user["password"].asString();

        if (strName.empty() || strPassword.empty())
        {
            std::cout << "用户名或密码为空" << std::endl;
            return false;
        }

        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_mysql.executeQuery("select id,score,total_count,win_count from user where username='" + strName + "' and password='" + strPassword + "'");

            res = m_mysql.get_mysql_store_result();
        }
        // 查询数据的结果要么只有一个数据 要么没有数据

        if (res == nullptr)
        {
            std::cout << "查询失败" << "用户名 :" << strName << "密码:" << strPassword << std::endl;
            return false;
        }

        int rowCount = mysql_num_rows(res);

        if (rowCount != 1)
        {
            std::cout << "结果不唯一数量为" << rowCount << std::endl;
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);

        user["id"] = std::stol(row[0]);
        user["score"] = std::stoi(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        std::cout << "登录成功" << "用户名 :" << strName << " 密码:" << strPassword << std::endl;
        return true;
    }

    /// @brief 根据传入的用户名去寻找数据库中是否存在该用户，并将其信息存入user中
    /// @param username 用户名
    /// @param user 传出型参数,如果找到的话 返回该用户的id,score,total_count,win_count
    /// @return 如果存在该用户则返回true，否则返回false
    bool select_byUsername(const std::string &username, Json::Value &user)
    {
        MYSQL_RES *res = nullptr;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_mysql.executeQuery("select id,score,total_count,win_count from user where username='" + username + "'");

            res = m_mysql.get_mysql_store_result();
        }

        if (res == nullptr)
        {
            std::cout << "查询失败" << "用户名 :" << username << std::endl;
            return false;
        }

        int rowCount = mysql_num_rows(res);

        if (rowCount != 1)
        {
            std::cout << "结果不唯一数量为" << rowCount << std::endl;
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = std::stol(row[0]);
        user["score"] = std::stoi(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);

        return true;
    }

    bool selectById(const uint64_t id, Json::Value &user)
    {
        MYSQL_RES *res = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_mysql.executeQuery("select id,username,score,total_count,win_count from user where id='" + std::to_string(id) + "'");
            INFO_LOG("select id,username,score,total_count,win_count from user where id= %ld", id);
            res = m_mysql.get_mysql_store_result();
        }

        if (res == nullptr)
        {
            std::cout << "查询失败" << "用户ID :" << id << std::endl;
            return false;
        }

        int rowCount = mysql_num_rows(res);
        if (rowCount != 1)
        {
            std::cout << "结果不唯一数量为" << rowCount << std::endl;
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = std::stol(row[0]);
        user["username"] = row[1];
        user["score"] = std::stoi(row[2]);
        user["total_count"] = std::stoi(row[3]);
        user["win_count"] = std::stoi(row[4]);

        
        return true;
    }

    bool win(const uint64_t id)
    {
        Json::Value user;
        selectById(id, user);

        m_mysql.executeQuery("update user set score=" + std::to_string(user["score"].asInt() + 10) + ",total_count=" + std::to_string(user["total_count"].asInt() + 1) + ",win_count=" + std::to_string(user["win_count"].asInt() + 1) + " where id=" + std::to_string(id) + ";");

        return true;
    }
    bool lose(const uint64_t id)
    {
        Json::Value user;
        selectById(id, user);

        m_mysql.executeQuery("update user set score=" + std::to_string(user["score"].asInt() - 10) + ",total_count=" + std::to_string(user["total_count"].asInt() + 1) + " where id=" + std::to_string(id) + ";");
        return true;
    }

private:
    MySQL_Util &m_mysql;
    std::mutex m_mutex;
};