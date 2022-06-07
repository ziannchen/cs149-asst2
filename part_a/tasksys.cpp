#include "tasksys.h"
#include <stdio.h>
#include <atomic>
#include <thread>
#include <iostream>
IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name() { return "Serial"; }

TaskSystemSerial::TaskSystemSerial(int num_threads)
    : ITaskSystem(num_threads) {}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable,
                                          int num_total_tasks,
                                          const std::vector<TaskID> &deps) {
    return 0;
}

void TaskSystemSerial::sync() { return; }

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads)
    : ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    num_threads_ = num_threads;
    thread_pool_ = new std::thread[num_threads];
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() { delete[] thread_pool_; }

void TaskSystemParallelSpawn::threadRun(IRunnable *runnable,
                                        int num_total_tasks,
                                        std::atomic<int> &curr_task_id) {
    int curr_task_turn = curr_task_id.fetch_add(1);
    while (curr_task_turn < num_total_tasks) {
        runnable->runTask(curr_task_turn, num_total_tasks);
        curr_task_turn = curr_task_id.fetch_add(1);
    }
}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    std::cout << "123" << std::endl;
    std::atomic<int> curr_task_id(0);
    for (int i = 0; i < num_threads_; i++) {
        thread_pool_[i] =
            std::thread(&TaskSystemParallelSpawn::threadRun, this, runnable,
                        num_total_tasks, std::ref(curr_task_id));
    }
    for (int i = 0; i < num_threads_; i++) {
        thread_pool_[i].join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
    return 0;
}

void TaskSystemParallelSpawn::sync() { return; }

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(
    int num_threads)
    : ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    num_threads_ = num_threads;
    num_finished_tasks_ = -1;
    hasTasks = false;
    thread_pool_ = new std::thread[num_threads];
    stopped = false;
    task_mu = new std::mutex();
    cond = new std::condition_variable();
    finished_mu = new std::mutex();
    runnable_ = nullptr;
    for (int i = 0; i < num_threads_; i++) {
        thread_pool_[i] = std::thread(
            &TaskSystemParallelThreadPoolSpinning::threadRunSpinning, this);
    }
}

void TaskSystemParallelThreadPoolSpinning::threadRunSpinning() {
    while (!stopped) {
        task_mu->lock();
        if (num_finished_tasks_ == -1 ||
            num_finished_tasks_ >= num_total_tasks_) {
            task_mu->unlock();
            continue;
        }
        int task_id = curr_task_id++;
        int num_total_tasks = num_total_tasks_;
        task_mu->unlock();
        // std::cerr << "curr taskid " << curr_task_id << "start" << std::endl;
        runnable_->runTask(task_id, num_total_tasks);
        // std::cerr << "curr taskid " << curr_task_id << "done" << std::endl;
        task_mu->lock();
        num_finished_tasks_++;
        if (num_finished_tasks_ >= num_total_tasks_) {
            finished_mu->unlock();
            cond->notify_all();
        }
        task_mu->unlock();
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    // std::cerr << "1" << std::endl;
    stopped = true;
    for (int i = 0; i < num_threads_; i++) {
        thread_pool_[i].join();
    }

    delete[] thread_pool_;
    delete task_mu;
    delete cond;
    delete finished_mu;
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable,
                                               int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    // std::cerr << "total " << num_total_tasks << std::endl;
    std::unique_lock<std::mutex> locker(*finished_mu);
    task_mu->lock();
    runnable_ = runnable;
    num_total_tasks_ = num_total_tasks;
    num_finished_tasks_ = 0;
    curr_task_id = 0;
    hasTasks = true;
    task_mu->unlock();

    while (num_finished_tasks_ < num_total_tasks_) {
        cond->wait(locker);
    }

    hasTasks = false;
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() { return; }

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(
    int num_threads)
    : ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable,
                                               int num_total_tasks) {

    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps) {

    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in
    // Part B.
    //

    return;
}
