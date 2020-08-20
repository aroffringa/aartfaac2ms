#ifndef STATIC_FOR_H
#define STATIC_FOR_H

#include "barrier.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <sched.h>

namespace aocommon {

template <typename Iter>
class StaticFor {
 public:
  StaticFor(size_t nThreads)
      : _nThreads(nThreads),
        _barrier(nThreads, [&]() { _hasTasks = false; }),
        _stop(false),
        _hasTasks(false) {}

  ~StaticFor() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_threads.empty()) {
      _stop = true;
      _hasTasks = true;
      _conditionChanged.notify_all();
      lock.unlock();
      for (std::thread &thr : _threads) thr.join();
    }
  }

  /**
   * Iteratively call a function in parallel.
   *
   * The function is expected to accept two size_t parameters, the loop
   * index and the thread id, e.g.:
   *   void loopFunction(size_t chunkStart, size_t chunkEnd);
   */
  void Run(Iter start, Iter end, std::function<void(Iter, Iter)> function) {
    Run(start, end, _nThreads, std::move(function));
  }

  /**
   * Iteratively call a function in parallel.
   *
   * The function is expected to accept two size_t parameters, the loop
   * index and the thread id, e.g.:
   *   void loopFunction(size_t chunkStart, size_t chunkEnd);
   */
  void Run(Iter start, Iter end, size_t nChunks,
           std::function<void(Iter, Iter)> function) {
    if (end == start + 1) {
      function(start, end);
    } else {
      if (_threads.empty()) startThreads();
      std::unique_lock<std::mutex> lock(_mutex);
      _iterStart = start;
      _iterEnd = end;
      _currentChunk = 0;
      _nChunks = std::min(nChunks, std::min(end - start, _nThreads));
      _loopFunction = std::move(function);
      _hasTasks = true;
      _conditionChanged.notify_all();
      lock.unlock();
      loop();
      _barrier.wait();
    }
  }

  size_t NThreads() const { return _nThreads; }

  /**
   * This method is only allowed to be called before Run() is
   * called.
   */
  void SetNThreads(size_t nThreads) {
    if (_threads.empty()) {
      _nThreads = nThreads;
      _barrier = Barrier(nThreads, [&]() { _hasTasks = false; });
    } else {
      throw std::runtime_error("Can not set NThreads after calling Run()");
    }
  }

 private:
  StaticFor(const StaticFor &) = delete;

  void loop() {
    size_t chunk;
    while (next(chunk)) {
      Iter chunkStart = _iterStart + (_iterEnd - _iterStart) * chunk / _nChunks;
      Iter chunkEnd =
          _iterStart + (_iterEnd - _iterStart) * (chunk + 1) / _nChunks;
      _loopFunction(chunkStart, chunkEnd);
    }
  }

  void run() {
    waitForTasks();
    while (!_stop) {
      loop();
      _barrier.wait();
      waitForTasks();
    }
  }

  bool next(size_t &chunk) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_currentChunk == _nChunks)
      return false;
    else {
      chunk = _currentChunk;
      ++_currentChunk;
      return true;
    }
  }

  void waitForTasks() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_hasTasks) _conditionChanged.wait(lock);
  }

  void startThreads() {
    if (_nThreads > 1) {
      _threads.reserve(_nThreads - 1);
      for (unsigned t = 1; t != _nThreads; ++t)
        _threads.emplace_back(&StaticFor::run, this);
    }
  }

  size_t _currentChunk, _nChunks;
  Iter _iterStart, _iterEnd;
  std::mutex _mutex;
  size_t _nThreads;
  Barrier _barrier;
  std::atomic<bool> _stop;
  bool _hasTasks;
  std::condition_variable _conditionChanged;
  std::vector<std::thread> _threads;
  std::function<void(Iter, Iter)> _loopFunction;
};

template <typename Iter>
class StaticTFor {
 public:
  StaticTFor(size_t nThreads)
      : _nThreads(nThreads),
        _barrier(nThreads, [&]() { _hasTasks = false; }),
        _stop(false),
        _hasTasks(false) {}

  ~StaticTFor() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_threads.empty()) {
      _stop = true;
      _hasTasks = true;
      _conditionChanged.notify_all();
      lock.unlock();
      for (std::thread &thr : _threads) thr.join();
    }
  }

  /**
   * Iteratively call a function in parallel.
   *
   * The function is expected to accept two size_t parameters, the loop
   * index and the thread id, e.g.:
   *   void loopFunction(size_t chunkStart, size_t chunkEnd);
   */
  void Run(Iter start, Iter end,
           std::function<void(Iter, Iter, size_t)> function) {
    Run(start, end, _nThreads, std::move(function));
  }

  /**
   * Iteratively call a function in parallel.
   *
   * The function is expected to accept two size_t parameters, the loop
   * index and the thread id, e.g.:
   *   void loopFunction(size_t chunkStart, size_t chunkEnd);
   */
  void Run(Iter start, Iter end, size_t nChunks,
           std::function<void(Iter, Iter, size_t)> function) {
    if (end == start + 1) {
      function(start, end, 0);
    } else {
      if (_threads.empty()) startThreads();
      std::unique_lock<std::mutex> lock(_mutex);
      _iterStart = start;
      _iterEnd = end;
      _currentChunk = 0;
      _nChunks = std::min(nChunks, std::min(end - start, _nThreads));
      _loopFunction = std::move(function);
      _hasTasks = true;
      _conditionChanged.notify_all();
      lock.unlock();
      loop(0);
      _barrier.wait();
    }
  }

  size_t NThreads() const { return _nThreads; }

  /**
   * This method is only allowed to be called before Run() is
   * called.
   */
  void SetNThreads(size_t nThreads) {
    if (_threads.empty()) {
      _nThreads = nThreads;
      _barrier = Barrier(nThreads, [&]() { _hasTasks = false; });
    } else {
      throw std::runtime_error("Can not set NThreads after calling Run()");
    }
  }

 private:
  StaticTFor(const StaticTFor &) = delete;

  void loop(size_t threadId) {
    size_t chunk;
    while (next(chunk)) {
      Iter chunkStart = _iterStart + (_iterEnd - _iterStart) * chunk / _nChunks;
      Iter chunkEnd =
          _iterStart + (_iterEnd - _iterStart) * (chunk + 1) / _nChunks;
      _loopFunction(chunkStart, chunkEnd, threadId);
    }
  }

  void run(size_t threadId) {
    waitForTasks();
    while (!_stop) {
      loop(threadId);
      _barrier.wait();
      waitForTasks();
    }
  }

  bool next(size_t &chunk) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_currentChunk == _nChunks)
      return false;
    else {
      chunk = _currentChunk;
      ++_currentChunk;
      return true;
    }
  }

  void waitForTasks() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_hasTasks) _conditionChanged.wait(lock);
  }

  void startThreads() {
    if (_nThreads > 1) {
      _threads.reserve(_nThreads - 1);
      for (unsigned t = 1; t != _nThreads; ++t)
        _threads.emplace_back(&StaticTFor::run, this, t);
    }
  }

  size_t _currentChunk, _nChunks;
  Iter _iterStart, _iterEnd;
  std::mutex _mutex;
  size_t _nThreads;
  Barrier _barrier;
  std::atomic<bool> _stop;
  bool _hasTasks;
  std::condition_variable _conditionChanged;
  std::vector<std::thread> _threads;
  std::function<void(Iter, Iter, size_t)> _loopFunction;
};
}  // namespace aocommon

#endif
