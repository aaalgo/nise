#include "extract.h"


    void Sift::extract(Image &image, std::vector<Feature> *list) {
        float *fdata = new float[image.cv->width * image.cv->height];
        BOOST_VERIFY(fdata);
        {
            float *d = fdata;
            unsigned char *l = (unsigned char *)image.cv->imageData;
            for (int h = 0; h < image.cv->height; ++h) {
                for (int w = 0; w < image.cv->width; ++w) {
                    *d = float(l[w]);
                    d++;
                }
                l += image.cv->widthStep;
            }
        }

        VlSiftFilt *filt = vl_sift_new (image.cv->width, image.cv->height, O, S, omin) ;

        BOOST_VERIFY(filt);
        if (edge_thresh >= 0) vl_sift_set_edge_thresh (filt, edge_thresh) ;
        if (peak_thresh >= 0) vl_sift_set_peak_thresh (filt, peak_thresh) ;
        if (magnif      >= 0) vl_sift_set_magnif      (filt, magnif) ;

        magnif = vl_sift_get_magnif(filt);

        list->clear();

        int err = vl_sift_process_first_octave (filt, fdata);
        while (err == 0) {
          
            vl_sift_detect (filt) ;
            
            VlSiftKeypoint const *keys  = vl_sift_get_keypoints(filt) ;
            int nkeys = vl_sift_get_nkeypoints(filt) ;

            for (int i = 0; i < nkeys ; ++i) {
                VlSiftKeypoint const *key = keys + i;
                double                angles [4] ;

                int nangles = 0;
                if (do_angle) {
                    nangles = vl_sift_calc_keypoint_orientations(filt, angles, key) ;
                }
                else {
                    nangles = 1;
                    angles[0] = 0;
                }

                for (int q = 0 ; q < nangles ; ++q) {

                    list->push_back(Feature());
                    Feature &f = list->back();
                    f.desc.resize(DIM);
                  /* compute descriptor (if necessary) */
                    vl_sift_calc_keypoint_descriptor(filt, &f.desc[0], key, angles[q]) ;

                    BOOST_FOREACH(float &v, f.desc) {
                        /*
                        v = round(v * SIFT_RANGE);
                        if (v > SIFT_RANGE) v = SIFT_RANGE;
                        */
                        v *= 2;
                        if (v > 1.0) v = 1.0;
                    }

                    f.scale = image.getScale();
                    f.x = key->x;
                    f.y = key->y;
                    f.dir = angles[q] / M_PI / 2;
                    f.size = key->sigma * magnif;

                    if ((entropy(f.desc) < e_th) || !checkBlackList(f.desc)) {
                        list->pop_back();
                    }
                   
                }
            }
            err = vl_sift_process_next_octave  (filt) ;
        }
        vl_sift_delete (filt) ;
        delete [] fdata;
    }
