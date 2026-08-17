#include "logger.hpp"
#include "util.hpp"

int main()
{
    // MySQL_Util &SQL = MySQL_Util::getInstance();
    // SQL.executeQuery("SELECT * FROM stu");
    // MYSQL_RES *result = mysql_store_result(SQL.getConnection());
    // int num_rows = mysql_num_rows(result);
    // INFO_LOG("Number of rows: %d", num_rows);
    // int num_cols = mysql_num_fields(result);
    // INFO_LOG("Number of columns: %d\n", num_cols);
    // for (int i = 0; i < num_rows; i++)
    // {
    //     MYSQL_ROW row = mysql_fetch_row(result);
    //     for (int j = 0; j < num_cols; j++)
    //     {
    //         printf("%s\t", row[j] ? row[j] : "NULL");
    //     }
    //     printf("\n");
    // }

    Json::Value root; // 创建一个根节点

    root["name"] = "John Doe";  // 添加字符串类型的数据
    root["age"] = 30;           // 添加整数类型的数据
    root["is_student"] = false; // 添加布尔类型的数据
    root["grades"].append(90);  // 添加数组类型的数据
    root["grades"].append(85);

    std::cout << Json_Util::serializeJson(root);
}