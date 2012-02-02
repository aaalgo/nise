#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <vector>
#include <boost/assert.hpp>
#include "image.h"

namespace nise {

    static void decompress_error_exit (j_common_ptr ptr) {
        j_decompress_ptr cinfo = reinterpret_cast<j_decompress_ptr>(ptr);
        char msg[JMSG_LENGTH_MAX];
        (*cinfo->err->format_message)(ptr, msg);
        jpeg_destroy_decompress(cinfo);
        throw ImageDecodingException(msg);
    }

    static void compress_error_exit (j_common_ptr ptr) {
        j_compress_ptr cinfo = reinterpret_cast<j_compress_ptr>(ptr);
        char msg[JMSG_LENGTH_MAX];
        (*cinfo->err->format_message) (ptr, msg);
        cinfo->dest->term_destination(cinfo);
        void **buf = reinterpret_cast<void **>(cinfo->client_data);
        if (*buf) free(*buf);
        jpeg_destroy_compress(cinfo);
        throw ImageEncodingException(msg);
    }

    void LoadJPEG (const std::string &buffer, CImg<unsigned char> *img) {
        BOOST_VERIFY(sizeof(JSAMPLE) == sizeof(unsigned char));
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jerr.error_exit = decompress_error_exit;
        jpeg_create_decompress(&cinfo);

        jpeg_mem_src(&cinfo, const_cast<unsigned char*>(reinterpret_cast<const unsigned char *>(&buffer[0])), buffer.size());

        jpeg_read_header(&cinfo, TRUE);
        // if only meta data is needed, goto  shortcut

        //cinfo.out_color_space
        // set out_color_space

        jpeg_start_decompress(&cinfo);

        const unsigned int row_stride = cinfo.output_width * cinfo.output_components;
        std::vector<unsigned char> buf(cinfo.output_height*row_stride);

        JSAMPROW row_pointer[1];
        while (cinfo.output_scanline < cinfo.output_height) {
            row_pointer[0] = &buf[0] + cinfo.output_scanline*row_stride;
            jpeg_read_scanlines(&cinfo,row_pointer,1);
        }
        jpeg_finish_decompress(&cinfo);

    //shortcut:
        jpeg_destroy_decompress(&cinfo);

        CImg<unsigned char> dest(cinfo.output_width,
                                 cinfo.output_height,
                                 1,
                                 cinfo.output_components);

        const unsigned char *buf2 = &buf[0];
        switch (dest.spectrum()) {
            case 1: {
                        unsigned char *ptr_g = dest.data(0,0,0,0);
                        cimg_foroff(dest,off) *(ptr_g++) = *(buf2++);
                    }
                    break;
            case 3: {
                        unsigned char 
                            *ptr_r = dest.data(0,0,0,0),
                            *ptr_g = dest.data(0,0,0,1),
                            *ptr_b = dest.data(0,0,0,2);
                        cimg_forXY(dest,x,y) {
                            *(ptr_r++) = *(buf2++);
                            *(ptr_g++) = *(buf2++);
                            *(ptr_b++) = *(buf2++);
                        }
                    }
                    break;
            case 4: {
                        unsigned char
                            *ptr_r = dest.data(0,0,0,0),
                            *ptr_g = dest.data(0,0,0,1),
                            *ptr_b = dest.data(0,0,0,2),
                            *ptr_a = dest.data(0,0,0,3);
                        cimg_forXY(dest,x,y) {
                            *(ptr_r++) = *(buf2++);
                            *(ptr_g++) = *(buf2++);
                            *(ptr_b++) = *(buf2++);
                            *(ptr_a++) = *(buf2++);
                        }
                    }
                    break;
            default:
                    throw ImageDecodingException("invalid spectrum");
        }
        dest.move_to(*img);
    }


    void EncodeJPEG (const CImg<unsigned char> &img,
                     std::string *buffer,
                     int quality) {
        BOOST_VERIFY(sizeof(JSAMPLE) == sizeof(unsigned char));

        J_COLOR_SPACE colortype=JCS_RGB;
        std::vector<unsigned char> buf;
        unsigned row_stride = 0;
        switch (img.spectrum()) {
            case 1: {
                        buf.resize(img.width() * img.height());
                        colortype = JCS_GRAYSCALE;
                        row_stride = img.width();
                        unsigned char *buf2 = &buf[0];
                        const unsigned char *ptr_g = img.data();
                        cimg_foroff(img,off) *(buf2++) = (JOCTET)*(ptr_g++);
                    }
                    break;
            case 3: {
                        buf.resize(img.width() * img.height() * 3);
                        colortype = JCS_RGB;
                        row_stride = img.width() * 3;
                        unsigned char *buf2 = &buf[0];
                        const unsigned char 
                            *ptr_r = img.data(0,0,0,0),
                            *ptr_g = img.data(0,0,0,1),
                            *ptr_b = img.data(0,0,0,2);
                        cimg_forXY(img,x,y) {
                          *(buf2++) = *(ptr_r++);
                          *(buf2++) = *(ptr_g++);
                          *(buf2++) = *(ptr_b++);
                        }
                    }
                    break;
            default:
                    throw ImageDecodingException("colorspace not supported");
        }

        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jerr.error_exit = compress_error_exit;
        jpeg_create_compress(&cinfo);

        unsigned char *out_buf = 0;
        long unsigned out_size = 0;


        jpeg_mem_dest(&cinfo, &out_buf, &out_size);
        cinfo.client_data = &out_buf;
        cinfo.image_width = img.width();
        cinfo.image_height = img.height();
        cinfo.input_components = img.spectrum();
        cinfo.in_color_space = colortype;
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, TRUE);

        jpeg_start_compress(&cinfo,TRUE);
 
        //const unsigned int row_stride = width()*dimbuf;
        unsigned char* row_pointer[1];

        while (cinfo.next_scanline < cinfo.image_height) {
            row_pointer[0] = &buf[cinfo.next_scanline*row_stride];
            jpeg_write_scanlines(&cinfo,row_pointer,1);
        }
        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);

        buffer->assign(reinterpret_cast<char *>(out_buf), out_size);
        free(out_buf);
    }
}
