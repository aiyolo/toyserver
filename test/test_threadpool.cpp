#include <bits/stdc++.h>
#include "../src/threadpool.hpp"
#include <chrono>

void func(int i)
{
    std::cout << "func " << i << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main()
{
    ThreadPool<std::function<void()>> pool(10);
    for (int i = 0; i < 100; i++)
    {
        pool.append(std::bind(func, i));
    }
    std::cout << "Hello World!" << std::endl;
    while (1);
}