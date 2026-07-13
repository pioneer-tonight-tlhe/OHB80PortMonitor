#ifndef SINGLETON_H
#define SINGLETON_H

#include <iostream>
#include <memory>
#include <mutex>

template <typename T>
class Singleton
{
protected:
    Singleton() = default;
    Singleton(const Singleton<T> &) = delete;
    Singleton &operator=(const Singleton<T> &) = delete;

public:
    static std::shared_ptr<T> getInstance()
    {
        static std::once_flag s_flag;
        std::call_once(s_flag, []() {
            m_instance = std::shared_ptr<T>(new T);
        });

        return m_instance;
    }

    void printAddress()
    {
        std::cout << m_instance.get() << std::endl;
    }

    ~Singleton()
    {
        std::cout << "this is singleton destruct" << std::endl;
    }

private:
    static std::shared_ptr<T> m_instance;
};

template <typename T>
std::shared_ptr<T> Singleton<T>::m_instance = nullptr;

#endif // SINGLETON_H
