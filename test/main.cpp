#include <iostream>
#include <unistd.h>
#include "../src/db/connection_pool.hpp"

void mainLoop(PGconn* conn);
void handleResult(PGresult* res, bool print_table = false);
void stressTest(db::connection_pool& pool);

void testIncorrectQueryies(db::connection_pool& pool);
void testCorrectQuery(db::connection_pool& pool);

int main(int argc, const char * argv[]) {
    
    db::connection_pool pool(10);
    stressTest(pool);
//    testIncorrectQueryies(pool);
//    testCorrectQuery(pool);
    pool.run({
        {"host", "localhost"},
        {"hostaddr", "127.0.0.1"},
        {"dbname", "sample"},
        {"user", "sample"},
        {"password", "123"}
    });

    std::cin.get();
    std::cout << "stopping" << std::endl;
    pool.stop();
    std::cout << "finish" << std::endl;
    return 0;
}
void testCorrectQuery(db::connection_pool& pool) {
    pool.async_query(db::query("SELECT * FROM users LIMIT 5", [](std::list<PGresult*> result){
        for(auto& r: result) {
            handleResult(r, true);
        }
    }));
}
void testIncorrectQueryies(db::connection_pool& pool) {
    pool.async_query(db::query("SELLLLL", [](std::list<PGresult*> result){
        for(auto& r: result) {
            handleResult(r);
        }
    }));
}

void stressTest(db::connection_pool& pool) {
    static std::atomic<int> total = 0;
    for(int i = 0; i < 20; ++i) {
        std::thread([&pool, i]{
            for (int j = 0; j < 10000; ++j) {
                pool.async_query(db::query(
                                            "INSERT INTO users(name, male) VALUES($1::text, $2::boolean)",
                                           {
                                               db::query::param::text(std::to_string(i * j)),
                                               db::query::param::boolean(i * j % 3)
                                           },
                                           [](std::list<PGresult*> results){
                    for(auto&r : results) {
                        handleResult(r);
                    }
                }));
                pool.async_query(db::query("SELECT * FROM users WHERE id=$1::bigint or male=$2::boolean", {
                    db::query::param::int64(3),
                    db::query::param::boolean(true),
                }, [i, j](std::list<PGresult*> results) {
                    //std::cout << i <<"." << j << " done" << std::endl;
                    for(auto&r : results) {
                        handleResult(r);
                    }
                    std::cout << ++total << std::endl;
                }));
            }
        }).detach();
    }
}

void handleResult(PGresult* result, bool print_table) {
    if (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_COMMAND_OK) {
        printf("handleResult: %s\n", PQresultErrorMessage(result));
        PQclear(result);
        return;
    }
    if (print_table) {
        int nTuples = PQntuples(result);
        if (nTuples > 0) {
            int nFields = PQnfields(result);
            printf("\n----------\n");
            for (int i = 0; i < nFields; ++i) {
                printf("%s\t", PQfname(result, i));
            }
            printf("\n----------\n");
            
            
            for (int i = 0; i < nTuples; ++i) {
                for (int j = 0; j < nFields; ++j) {
                    printf("%s\t", PQgetvalue(result, i, j));
                }
                printf("\n");
            }
        }
    }

    PQclear(result);
}
