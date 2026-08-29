#pragma once
#include "logger.hpp"
#include <mysql/mysql.h>
#include <memory>
#include <jsoncpp/json/json.h>
#include <vector>
#include <fstream>

struct MySQLDeleter;

class MySQL_Util
{
public:
    static MySQL_Util &getInstance()
    {
        static MySQL_Util instance;
        return instance;
    }

    bool executeQuery(const std::string &query)
    {
        if (mysql_query(m_mysql.get(), query.c_str()))
        {
            ERR_LOG("MySQL query failed: %s", query.c_str());
            std::cout << "MySQL query failed: " << mysql_error(m_mysql.get()) << std::endl;
        }
        return true;
    }

    MYSQL *getConnection()
    {
        return m_mysql.get();
    }

    MYSQL_RES *get_mysql_store_result()
    {
        return mysql_store_result(m_mysql.get());
    };

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

class Json_Util
{
public:
    static std::string serializeJson(const Json::Value &jsonValue)
    {
        Json::StreamWriterBuilder writer;
        return Json::writeString(writer, jsonValue) + "\n"; // 添加换行符
    }

    static Json::Value deserializeJson(const std::string &jsonString)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(jsonString, root))
        {
            throw std::runtime_error("Failed to parse JSON: " + reader.getFormattedErrorMessages());
        }
        return root;
    }
};

class string_Util
{
public:
    static std::vector<std::string> split(const std::string &str, const std::string &sep)
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = str.find(sep);

        while (start < str.length())
        {
            // 没找到
            if (end == std::string::npos)
            {
                result.push_back(str.substr(start));
                break;
            }
            if (end == start)
            {
                start = end + sep.length();
                continue; // 避免空字符串
            }
            result.push_back(str.substr(start, end - start));
            start = end + sep.length();
            end = str.find(sep, start);
        }

        return result;
    }
};

/// @brief 读取HTML文件返回给客户端,例如注册页面 登录页面
class file_Util
{
public:
    static std::string readFile(const std::string &strPathName)
    {
        // 打开文件
        std::ifstream ifs(strPathName, std::ios::binary); // 使用二进制读取文件可以避免读取文本文件时对一些数据进行错误的处理
        // 获取文件大小 把文件指针放到文件的末尾 然后获取相对于起始位置的偏移量
        size_t fileSize;
        if (ifs.is_open())
        {
            // seekg(off, dir) 的作用就是：以 dir 指定的位置为基准，将读取指针移动 off 个字节
            ifs.seekg(0, std::ios::end);
            fileSize = ifs.tellg();
            std::cout << "File size: " << fileSize << " bytes" << std::endl;
        }
        else
        {
            ERR_LOG("Failed to open file: %s", strPathName.c_str());
            ifs.close();
            return "";
        }

        ifs.seekg(0, std::ios::beg); // 将文件指针移动到文件开头

        std::string strRet;
        strRet.resize(fileSize);
        ifs.read(&strRet[0], fileSize);
        if (ifs.good())
        {
            // INFO_LOG("File read successfully: %s", strPathName.c_str());
        }
        else
        {
             // ERR_LOG("Failed to read file: %s", strPathName.c_str());
            ifs.close();
            return "";
        }

        ifs.close();
        return strRet;
    }
};