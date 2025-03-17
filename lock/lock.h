#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <atomic>
#include <thread>
class mutex_lock_t
{
public:
    mutex_lock_t()
    {
        assert(pthread_mutex_init(&mutex, NULL) == 0);
    }
    ~mutex_lock_t()
    {
        pthread_mutex_destroy(&mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        return &mutex;
    }

private:
    pthread_mutex_t mutex;
};

#include <atomic>
#include <thread>

class spin_mutex
{
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    spin_mutex() = default;
    spin_mutex(const spin_mutex &) = delete;
    spin_mutex &operator=(const spin_mutex &) = delete;
    void lock()
    {
        while (flag.test_and_set(std::memory_order_acquire))
            ;
    }
    void unlock()
    {
        flag.clear(std::memory_order_release);
    }
};

class sem
{
public:
    sem()
    {
        assert(sem_init(&m_sem, 0, 0) == 0);
    }
    sem(int num)
    {
        assert(sem_init(&m_sem, 0, num) == 0);
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

class cond_t
{
public:
    cond_t()
    {
        assert(pthread_cond_init(&m_cond, NULL) == 0);
    }
    ~cond_t()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)
    {
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};
