//
// Created by Maksim Ruts on 29-Aug-18.
//

#include "texture.h"

Texture2d::Texture2d(const std::string& img_data, const GLuint width, const GLuint height) :
    data{img_data},
    width{width},
    height{height},
    internal_format{GL_RGB},
    image_format{GL_RGBA},
    wrap_s{GL_REPEAT},
    wrap_t{GL_REPEAT},
    filter_min{GL_LINEAR},
    filter_max{GL_LINEAR}
{
    generate();
}

Texture2d::Texture2d(const std::string& img_data, const GLuint width, const GLuint height, const GLuint img_format) :
    data{img_data},
    width{width},
    height{height},
    internal_format{img_format},
    image_format{img_format},
    wrap_s{GL_REPEAT},
    wrap_t{GL_REPEAT},
    filter_min{GL_LINEAR},
    filter_max{GL_LINEAR}
{
    generate();
}

void Texture2d::generate() {
    // create texture
    glBindTexture(GL_TEXTURE_2D, obj_id);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, image_format, GL_UNSIGNED_BYTE, data.c_str());
    // set wrap mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
    // set filter mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_max);
    // unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

 const GLuint& Texture2d::get_obj_id() const {
    return obj_id;
}