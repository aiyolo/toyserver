#include <iostream>
#include <mysql/mysql.h>

int main(){
    MYSQL* conn = mysql_init(NULL);
    conn = mysql_real_connect(conn, "localhost", "root", "tobeNo.1", "mydb", 3306, NULL, 0);
    if(conn == NULL){
        std::cout << "connect error" << std::endl;
        return 1;
    }
    std::cout << "connect success" << std::endl;
    int res = mysql_query(conn, "select * from user");
    if(res != 0){
        std::cout << "query error" << std::endl;
        return 1;
    }
    MYSQL_RES* res_ptr = mysql_store_result(conn);
    unsigned int num_fields = mysql_num_fields(res_ptr);
    MYSQL_ROW row; // 字符串数组
    while((row = mysql_fetch_row(res_ptr)) != NULL){
        for(unsigned int i = 0; i < num_fields; i++){
            std::cout << row[i] << " ";
        }
        std::cout << std::endl;
    }
    mysql_free_result(res_ptr);
    mysql_close(conn);
}