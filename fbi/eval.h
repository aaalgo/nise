#include <sys/time.h>
#include <limits>

namespace fbi {

class Timer
{
    struct  timeval start; 
public:
    Timer () { restart(); }
    /// Start timing.
    void restart ()
    {
        gettimeofday(&start, 0); 
    }
    /// Stop timing, return the time passed (in second).
    float elapsed () const
    {
        struct timeval end;
        float   diff; 
        gettimeofday(&end, 0); 

        diff = (end.tv_sec - start.tv_sec) 
                + (end.tv_usec - start.tv_usec) * 0.000001; 
        return diff;
    }
};

class Stat
{
    int count;
    float sum;
    float sum2;
    float min;
    float max;
public:
    Stat () : count(0), sum(0), sum2(0), min(std::numeric_limits<float>::max()), max(-std::numeric_limits<float>::max()) {
    }

    ~Stat () {
    }

    void reset () {
        count = 0;
        sum = sum2 = 0;
        min = std::numeric_limits<float>::max();
        max = -std::numeric_limits<float>::max();
    }

    void append (float r)
    {
        count++;
        sum += r;
        sum2 += r*r;
        if (r > max) max = r;
        if (r < min) min = r;
    }

    Stat & operator<< (float r) { append(r); return *this; }

    int getCount() const { return count; }
    float getSum() const { return sum; }
    float getAvg() const { return sum/count; }
    float getMax() const { return max; }
    float getMin() const { return min; }
    float getStd() const
    {
        if (count > 1) return std::sqrt((sum2 - (sum/count) * sum)/(count - 1)); 
        else return 0; 
    }
};

}
