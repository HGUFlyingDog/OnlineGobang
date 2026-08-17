#pragma once
#include "logger.hpp"
#include <mysql/mysql.h>
#include <memory>

struct MySQLDeleter;

class MySQL_Util
{
public:
    static MySQL_Util &getInstance()
    {
        static MySQL_Util instance;
        return instance;
    }

    void executeQuery(const std::string &query)
    {
        if (mysql_query(m_mysql.get(), query.c_str()))
        {
            throw std::runtime_error("mysql_query failed: " + std::string(mysql_error(m_mysql.get())));
        }
    }

    MYSQL *getConnection()
    {
        return m_mysql.get();
    }

private:
    MySQL_Util() : m_mysql(mysql_init(nullptr)) // 初始化数据库
    {
        if (!m_mysql)
            throw std::runtime_error("mysql_init failed");
        if (mysql_real_connect(m_mysql.get(), "localhost", "", "", "gobang", 0, NULL, 0) == NULL) // 连接数据库
            throw std::runtime_error("mysql_real_connect failed");
        mysql_set_character_set(m_mysql.get(), "utf8"); // 设置字符集
        mysql_select_db(m_mysql.get(), "gobang");       // 选择数据库

        INFO_LOG("MySQL_Util initialized successfully");
    }

    struct MySQLDeleter
    {
        void operator()(MYSQL *conn) const
        {
            if (conn)
                mysql_close(conn);

            INFO_LOG("MySQL connection closed");
        }
    };

private:
    // 析构的时候会调用 MySQLDeleter{}(m_mysql.get());
    std::unique_ptr<MYSQL, MySQLDeleter> m_mysql;
};