#pragma once

#include <functional>
#include <unordered_map>

namespace containers {
class Parse {
   public:
    Parse() = default;

   public:
    template <typename F, typename... Args>
    void insert(size_t num, F&& f, Args&&... args) {
        tasks_[num] = [f = std::forward<F>(f), ... args = std::forward<Args>(args)]() mutable {
            std::invoke(f, std::move(args)...);
        };
    }
    void erase(size_t num) { tasks_.erase(num); }
    void execute(size_t num) const { tasks_.at(num)(); };

    bool contains(size_t num) const { return tasks_.contains(num); }

   private:
    std::unordered_map<size_t, std::function<void()>> tasks_;
};

}  // namespace containers
