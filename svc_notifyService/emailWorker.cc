#include "emailWorker.h"
#include <bite_scaffold/log.h>

namespace notifyService {

EmailWorker::EmailWorker() : _running(false) {
}

EmailWorker::~EmailWorker() {
    stop();
}

void EmailWorker::start() {
    // 1. 检查线程是否正在运行
    if (_running.load()) {
        return;
    }

    // 2. 启动线程
    _running.store(true);
    _workerThread = std::thread(&EmailWorker::workerThread, this);
}

void EmailWorker::stop() {
    // 1. 检查线程是否正在运行
    if (!_running.load()) {
        return;
    }

    // 2. 设置退出标志
    _running.store(false);

    // 3. 通知线程退出循环
    _cond.notify_one();
    if (_workerThread.joinable()) {
        _workerThread.join();
    }
}

void EmailWorker::addTask(EmailTask task) {
    // 1. 将任务添加到队列
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _taskQueue.push(task);
    }
    // 2. 通知线程有任务可处理
    _cond.notify_one();
}

void EmailWorker::workerThread() {
    while (true) {
        // 1. 创建任务对象
        EmailTask task;
        // 2. 获取任务
        {
            // 2.1 阻塞等待，一旦队列中有任务添加，该线程就会被唤醒
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this] {
                return !_running.load() || !_taskQueue.empty();
            });

            // 2.2 检查线程是否退出
            if (!_running.load() && _taskQueue.empty()) {
                return;
            }

            // 2.3 从队列中获取任务
            task = _taskQueue.front();
            _taskQueue.pop();
        }

        // 3. 处理任务
        if (task._emailSender) {
            task._emailSender->sendEmail(task._toEmail, task._subject, task._content);
        }
    }
}

} // namespace notifyService