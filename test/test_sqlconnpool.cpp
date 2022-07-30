#include "../src/sqlconnpool.hpp"

using namespace std;

int main(){
    SqlConnPool* pool = SqlConnPool::getInstance();
    pool->init("localhost", "root", "tobeNo.1", "mydb", 3306, 10);
    cout << pool->getFreeConnCount() << endl;
    MYSQL* conn = pool->getConn();
    if(conn) cout << "get conn success" << endl;
    else cout << "get conn failed" << endl;
    std::cout << "connect success" << std::endl;
    // int res = mysql_query(conn, "select * from user");
    int res = mysql_query(conn, "select username, password from user where username='root'");
    if(res != 0){
        std::cout << mysql_error(conn);
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
    pool->releaseConn(conn);
    pool->destroy();
    return 0;
}