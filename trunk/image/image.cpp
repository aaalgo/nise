#include "image.h"

namespace nise {

    POCO_IMPLEMENT_EXCEPTION(BadImageFormatException, Poco::ApplicationException, "BAD IMAGE");
    POCO_IMPLEMENT_EXCEPTION(ImageDecodingException, Poco::ApplicationException, "IMAGE DECODING");
    POCO_IMPLEMENT_EXCEPTION(ImageEncodingException, Poco::ApplicationException, "IMAGE ENCODING");
    POCO_IMPLEMENT_EXCEPTION(ImageColorSpaceException, Poco::ApplicationException, "COLORSPACE");

}
