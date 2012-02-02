#ifndef WDONG_NISE_IMAGE
#define WDONG_NISE_IMAGE

#define cimg_display 0
#define cimg_debug 0

#include <string>
#include <Poco/Exception.h>
#include <CImg.h>
#include <lshkit.h>
#include "../common/nise.h"

namespace nise {

    POCO_DECLARE_EXCEPTION(, BadImageFormatException, Poco::ApplicationException);
    POCO_DECLARE_EXCEPTION(, ImageDecodingException, Poco::ApplicationException);
    POCO_DECLARE_EXCEPTION(, ImageEncodingException, Poco::ApplicationException);
    POCO_DECLARE_EXCEPTION(, ImageColorSpaceException, Poco::ApplicationException);

    using cimg_library::CImg;

    void LoadJPEG (const std::string &buffer, CImg<unsigned char> *img);
    void LoadGIF (const std::string &buffer, CImg<unsigned char> *img);
    void LoadPNG (const std::string &buffer, CImg<unsigned char> *img);
    void LoadImage (const std::string &buffer, CImg<unsigned char> *img);

    void EncodeJPEG (const CImg<unsigned char> &img, std::string *buffer, int quality = 90);

    template <typename T>
    float CImgLimitSize (CImg<T> *img, int max) {
        if ((img->width() <= max) && (img->height() <= max)) return 1.0;
        float scale;
        unsigned new_width, new_height;
        if (img->width() < img->height()) {
            scale = 1.0F * img->height() / max;
            new_width = img->width() * max / img->height();
            new_height = max;
        }
        else {
            scale = 1.0F * img->width() / max;
            new_width = max;
            new_height = img->height() * max / img->width();
        }
        img->resize(new_width, new_height);
        return scale;
    }

    template <typename T>
    float CImgLimitSizeBelow (CImg<T> *img, int min) {
        if ((img->width() >= min) && (img->height() >= min)) return 1.0;
        float scale;
        unsigned new_width, new_height;
        if (img->width() > img->height()) {
            scale = 1.0F * img->height() / min;
            new_width = img->width() * min / img->height();
            new_height = min;
        }
        else {
            scale = 1.0F * img->width() / min;
            new_width = min;
            new_height = img->height() * min / img->width();
        }
        img->resize(new_width, new_height);
        return scale;
    }

    template <typename T>
    void CImgToGray (const CImg<T> &img, CImg<float> *gray) {

      CImg<float> result(img.width(), img.height(), img.depth(), 1);

      if (img.spectrum() == 3) {
          const T *p1 = img.data(0,0,0,0),
                  *p2 = img.data(0,0,0,1),
                  *p3 = img.data(0,0,0,2);
          float *pr = result.data(0,0,0,0);
          for (unsigned int N = img.width()*img.height()*img.depth(); N; --N) {
            const float
              R = *(p1++),
              G = *(p2++),
              B = *(p3++);
              *(pr++) = 0.299f * R + 0.587f * G + 0.114f * B;
          }
      }
      else if (img.spectrum() == 1) {
          const T *p = img.data(0,0,0,0);
          float *pr = result.data(0,0,0,0);
          for (unsigned int N = img.width()*img.height()*img.depth(); N; --N) {
              *(pr++) = *(p++);
          }
      }
      else {
        throw ImageColorSpaceException("image is not a RGB or gray image.");
      }
      result.move_to(*gray);
    }

    class Sift {
        static const unsigned DIM = 128;
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
                float d = l2(&desc[0], &b[0]);
                if (d < threshold) return false;
            }
            return true;
        }
    public:
        struct Feature {
            Region region;
            std::vector<float> desc;
        };

        static float entropy (const std::vector<float> &desc) {
            std::vector<unsigned> count(256);
            fill(count.begin(), count.end(), 0);
            BOOST_FOREACH(float v, desc) {
                unsigned c = unsigned(floor(v * 255 + 0.5));
                if (c < 256) count[c]++;
            }
            double e = 0;
            BOOST_FOREACH(unsigned c, count) {
                if (c > 0) {
                    float pr = (float)c / (float)desc.size();
                    e += -pr * log(pr)/log(2.0);
                }
            }
            return float(e);
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

        void extract(const CImg<float> &image, float scale, std::vector<Feature> *list); 
    };

    static inline void binarify (const Sift::Feature &f, std::vector<char> *b, float qt) {
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

    static inline bool sift_by_size (const Sift::Feature &f1, const Sift::Feature &f2) {
        return f1.region.r > f2.region.r;
    }


    static inline void SampleFeature (std::vector<Sift::Feature> *sift, unsigned count, unsigned method) {
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

    static inline void logscale (std::vector<Sift::Feature> *v, float l) {
        float s = -log(l);
        BOOST_FOREACH(Sift::Feature &f, *v) {
            BOOST_FOREACH(float &d, f.desc) {
                if (d < l) d = l;
                d = (log(d) + s) / s;
                if (d > 1.0) d = 1.0;
            }
        }
    }



}

#endif
