#ifndef NISE_EXTRACT
#define NISE_EXTRACT
#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <boost/foreach.hpp>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <lshkit.h>
extern "C" {
#include "generic.h"
#include "sift.h"
}

/// Position of a feature
struct Feature {
    float x, y, size, dir;  // dir in [0,1]
    float scale;    // 
    std::vector<float> desc;
};

static const unsigned int BYTE_MAX = 256;

static inline std::istream &operator >> (std::istream &is, Feature &s) {
    unsigned dim;
    is >> s.x >> s.y >> s.size >> s.dir >> s.scale >>  dim;
    if (is) {
        s.desc.resize(dim);
        BOOST_FOREACH(float &v, s.desc) {
            is >> v;
        }
    }
    return is;
}

static inline std::ostream &operator << (std::ostream &os, const Feature &s) {
    os << s.x << '\t' << s.y << '\t' << s.size << '\t' << s.dir << '\t' << s.scale << '\t' << s.desc.size();
    BOOST_FOREACH(float v, s.desc) {
        os << '\t' << v;
    }
    return os;
}

static inline CvScalar rand_color () {
    unsigned R = 128;
    unsigned G = 128;
    unsigned B = 128;
    switch (rand() % 3) {
    case 0: R += rand() % 128; break;
    case 1: G += rand() % 128; break;
    case 2: B += rand() % 128; break;
    }
    return CV_RGB(R, G, B);
}

class Image {
    IplImage *cv;

    unsigned o_width;
    unsigned o_height;
    float scale;

public:
    Image (unsigned w, unsigned h, unsigned channel = 1) 
    {
        o_width = w;
        o_height = h;
        scale = 1.0;
        cv = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, channel);
        cvSetZero(cv);
    }

    Image (const std::string &path, int MAX = 0, bool color = false)
    {
        scale = 1.0;
        cv = cvLoadImage(path.c_str(), color ? CV_LOAD_IMAGE_COLOR : CV_LOAD_IMAGE_GRAYSCALE);
        //BOOST_VERIFY(cv != 0);
        if (cv == 0) throw std::runtime_error("ERROR LOAD IMAGE");

        o_width = cv->width;
        o_height = cv->height;

        if (MAX == 0) return;
        if (cv->width <= MAX && cv->height <= MAX) return;

        CvSize size;
        if (MAX < 0) {
            scale = -MAX;
            size.width = cv->width / scale;
            size.height = cv->height / scale;
        }
        else if (cv->width < cv->height) {
            scale = cv->height / MAX;
            size.width = cv->width * MAX / cv->height;
            size.height = MAX;
        }
        else {
            scale = cv->width / MAX;
            size.width = MAX;
            size.height = cv->height * MAX / cv->width;
        }
        IplImage *old = cv;
        cv = cvCreateImage(size, old->depth, old->nChannels);
        cvResize(old, cv);
        cvReleaseImage(&old);
    }

    Image (const char *buf, size_t len, int MAX = 0, bool color = false) {
        scale = 1.0;
        CvMat mat;
        cvInitMatHeader(&mat, 1, len, CV_8UC1, const_cast<char*>(buf), len);

        cv = cvDecodeImage(&mat, color? CV_LOAD_IMAGE_COLOR : CV_LOAD_IMAGE_GRAYSCALE);
        //BOOST_VERIFY(cv != 0);
        if (cv == 0) throw std::runtime_error("ERROR LOAD IMAGE");

        o_width = cv->width;
        o_height = cv->height;

        if (MAX == 0) return;
        if (cv->width <= MAX && cv->height <= MAX) return;


        CvSize size;
        if (cv->width < cv->height) {
            scale = cv->height / MAX;
            size.width = cv->width * MAX / cv->height;
            size.height = MAX;
        }
        else {
            scale = cv->width / MAX;
            size.width = MAX;
            size.height = cv->height * MAX / cv->width;
        }
        IplImage *old = cv;
        cv = cvCreateImage(size, old->depth, old->nChannels);
        cvResize(old, cv);
        cvReleaseImage(&old);
    }

    ~Image () {
        cvReleaseImage(&cv);
    }

    void save (const std::string &path) {
        cvSaveImage(path.c_str(), cv);
    }

    unsigned width () const {
        return cv->width;
    }

    unsigned height () const {
        return cv->height; 
    }

    unsigned orig_width () const {
        return o_width;
    }

    unsigned orig_height () const {
        return o_height;
    }

    float getScale () const {
        return scale;
    }

    void line (unsigned x1, unsigned y1, unsigned x2, unsigned y2, CvScalar color = CV_RGB(255,255,255)) {
        cvLine(cv, cvPoint(x1, y1),
                   cvPoint(x2, y2),
                   color);
    }

    void copy (const Image &im, unsigned x, unsigned y) {
        cvSetImageROI(cv,cvRect(x, y, im.cv->width, im.cv->height));
        if (cv->nChannels == 3) {
            cvSetImageCOI(cv, 1);
            cvCopy(im.cv, cv, 0);
            cvSetImageCOI(cv, 2);
            cvCopy(im.cv, cv, 0);
            cvSetImageCOI(cv, 3);
            cvCopy(im.cv, cv, 0);
            cvSetImageCOI(cv, 0);
        }
        else {
            cvCopy(im.cv, cv, 0);
        }
        cvResetImageROI(cv);
    }
    friend class Sift;

    void encode (std::string *data) {
        CvMat *mat = cvEncodeImage(".jpg", cv , 0);
        data->resize(mat->cols);
        std::copy(mat->data.ptr, mat->data.ptr + mat->cols, data->begin());
        cvReleaseMat(&mat);
    }
};

static inline void LoadFeatures (const std::string &path, std::vector<Feature> *list) {
    std::ifstream is(path.c_str());
    unsigned count;
    is >> count;
    if (is) {
        list->resize(count);
        BOOST_FOREACH(Feature &f, *list) {
            is >> f;
        }
    }
}

static inline void SaveFeatures (const std::string &path, const std::vector<Feature> list) {
    std::ofstream os(path.c_str());
    os << list.size() << std::endl;
    if (os) {
        BOOST_FOREACH(const Feature &f, list) {
            os << f << std::endl;
        }
    }
}

class Sift {
    static const unsigned DIM = SIFT_DIM;
    int O, S, omin;
    double edge_thresh;
    double peak_thresh;
    double magnif;
    double e_th;
    bool do_angle;
    unsigned R;

    std::vector<std::vector<float> > black_list;
    float threshold;

    bool checkBlackList (const std::vector<float> &desc) {
        lshkit::metric::l2<float> l2(Sift::dim());
        BOOST_FOREACH(const std::vector<float> &b, black_list) {
            // if (l2(&desc[0], &b[0]) < threshold) return false;
            float d = l2(&desc[0], &b[0]);
            if (d < threshold) return false;
        }
        return true;
    }
public:

    static float entropy (const std::vector<float> &desc) {
        std::vector<unsigned> count(256);
        fill(count.begin(), count.end(), 0);
        BOOST_FOREACH(float v, desc) {
            unsigned c = round(v * 255);
            if (c < 256) count[c]++;
        }
        float e = 0;
        BOOST_FOREACH(unsigned c, count) {
            if (c > 0) {
                float pr = (float)c / (float)desc.size();
                e += - pr * log2(pr);
            }
        }
        return e;
    }

    Sift (int O_ = -1, int S_ = 3, int omin_ = -1,
            double et_ = -1, double pt_ = 3, double mag_ = -1, double e_th_ = 0, bool do_angle_ = true, unsigned R_ = 256)
        : O(O_), S(S_), omin(omin_), edge_thresh(et_), peak_thresh(pt_), magnif(mag_), e_th(e_th_), do_angle(do_angle_), R(R_)
    {
    }

    void setBlackList (const std::string &path, float t) {
        threshold = t;
        std::vector<float> tmp(Sift::dim());
        std::ifstream is(path.c_str(), std::ios::binary);
        while (is.read((char *)&tmp[0], sizeof(float) * Sift::dim())) {
            black_list.push_back(tmp);
        }
    }

    static unsigned dim () {
        return DIM;
    }

    void extract(Image &image, std::vector<Feature> *list); 
};

static inline void binarify (const Feature &f, std::vector<char> *b, float qt) {
    unsigned dim = (f.desc.size() + 7) / 8;
    b->resize(dim);
    unsigned i = 0, j = 0;
    BOOST_FOREACH(float v, f.desc) {
        if (j == 0) {
            b->at(i) = 0;
        }
        if (v >= qt) b->at(i) |= (1 << j);
        ++j;
        if (j == 8) {
            j = 0;
            ++i;
        }
    }
}

static const unsigned SAMPLE_RANDOM = 0;
static const unsigned SAMPLE_SIZE = 1;

static inline bool sift_by_size (const Feature &f1, const Feature &f2) {
    return f1.size > f2.size;
}

static inline void SampleFeature (std::vector<Feature> *sift, unsigned count, unsigned method) {
    if (sift->size() < count) return;
    if (method == SAMPLE_RANDOM) {
        std::random_shuffle(sift->begin(), sift->end());
    }
    else if (method == SAMPLE_SIZE) {
        std::sort(sift->begin(), sift->end(), sift_by_size);
    }
    else {
        BOOST_VERIFY(0);
    }
    sift->resize(count);
}

static inline void logscale (std::vector<Feature> *v, float l) {
    float s = -log(l);
    BOOST_FOREACH(Feature &f, *v) {
        BOOST_FOREACH(float &d, f.desc) {
            if (d < l) d = l;
            d = (log(d) + s) / s;
            if (d > 1.0) d = 1.0;
        }
    }
}

#endif

