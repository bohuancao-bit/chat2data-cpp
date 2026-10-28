#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include "emailTask.h"

namespace notifyService {

class EmailWorker {
public:
    EmailWorker();
    ~EmailWorker();
    // 启动发送邮件线程
    void start();
    // 停止发送邮件线程
    void stop();
    // 添加发送邮件任务
    void addTask(EmailTask task);
private:
    // 发送邮件线程函数
    void workerThread();
private:
    std::queue<EmailTask> _taskQueue;  // 邮件任务队列
    std::mutex _mutex;                 // 互斥锁，作用保证任务队列线程安全访问
    std::condition_variable _cond;     // 条件变量，作用通知线程有任务可处理
    std::thread _workerThread;         // 发送邮件线程对象
    std::atomic<bool> _running;        // 原子类型变量，作用标记线程是否正在运行
};

} // namespace notifyService