#include <chrono>
namespace utils {

class Duration {
  private:
    std::chrono::duration<double> duration;

  public:
    Duration(const std::chrono::duration<double> &difference) {
        duration = difference;
    }

    operator double() const { return duration.count(); }
};

class Point {
  private:
    std::chrono::high_resolution_clock::time_point time_point;

  public:
    Point &
    operator=(const std::chrono::high_resolution_clock::time_point &rhs) {
        time_point = rhs;
        return *this;
    };

    Duration operator-(const Point &rhs) {
        return Duration(time_point - rhs.time_point);
    };
};

Point get_time() {
    Point tp;
    tp = std::chrono::high_resolution_clock::now();
    return tp;
}
} // namespace utils
