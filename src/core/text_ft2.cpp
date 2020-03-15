//
// Created by novasurfer on 12/31/19.
//

#include "text_ft2.h"
#include "debug_utils.h"
#include "log2.h"
#include "math/utils.h"
#include <math/transform.h>

namespace sc2d
{
    Vertex TextFt2::quad_vertices[VERTICES_PER_QUAD] {SPRITE_QUAD.tr, SPRITE_QUAD.br,
                                                      SPRITE_QUAD.bl, SPRITE_QUAD.tl};

    void Ft2Font::init(const char* font_path, uint32_t font_size, uint32_t chars_table_size)
    {
        height = font_size;
        chars_table_lenght = chars_table_size;

        if(FT_Init_FreeType(&ft))
            log_err_cmd("CAN NOT INIT FREETYPE.");

        FT_Face face;

        if(FT_New_Face(ft, font_path, 0, &face))
            log_info_cmd("FAILED TO LOAD FONT.");

        FT_Set_Pixel_Sizes(face, 0, font_size);

        // quick and dirty max texture size estimate
        texture_width = 1;
        uint32_t max_dim = (1 + (face->size->metrics.height >> 6)) * ceilf(sqrtf(chars_table_lenght));
        while(texture_width < max_dim)
            texture_width <<= 1;
        uint32_t tex_height = texture_width;

        // render glyphs to atlas
        pixels = (unsigned char*)calloc(texture_width * tex_height, 1);
        ascender = face->ascender >> 5;
        uint32_t pen_x = 0, pen_y = 0;

        for(size_t i = 32; i < chars_table_lenght; ++i) {
            // Maybe its better to use FL_LOAD_TARGET_LIGHT for regular, non-pixelart fonts.
            FT_Load_Char(face, i, FT_LOAD_RENDER);
            FT_Bitmap* bmp = &face->glyph->bitmap;

            if(pen_x + bmp->width >= texture_width) {
                pen_x = 0;
                pen_y += ((face->size->metrics.height >> 6) + 1);
            }

            for(uint32_t row = 0; row < bmp->rows; ++row) {
                for(uint32_t col = 0; col < bmp->width; ++col) {
                    uint32_t x = pen_x + col;
                    uint32_t y = pen_y + row;
                    pixels[y * texture_width + x] = bmp->buffer[row * bmp->pitch + col];
                }
            }

            glyph[i].x0 = pen_x;
            glyph[i].y0 = pen_y;
            glyph[i].x1 = pen_x + bmp->width;
            glyph[i].y1 = pen_y + bmp->rows;

            glyph[i].advance_x = face->glyph->advance.x >> 6;
            glyph[i].bearing_x = face->glyph->metrics.horiBearingX >> 6;
            glyph[i].bearing_y = face->glyph->metrics.horiBearingY >> 6;

            pen_x += bmp->width + 1;
        }
    }

    void TextFt2::init(const Shader& txt_shader, const Ft2Font& fnt)
    {
        shader = &txt_shader;
        font = &fnt;

        glGenTextures(1, &obj_id);
        glBindTexture(GL_TEXTURE_2D_ARRAY, obj_id);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, font->texture_width);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, font->texture_width);

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RED, font->height, font->height,
                     font->chars_table_lenght, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
//        log_gl_error_cmd();

        for(uint8_t i = 0; i < font->chars_table_lenght; ++i) {
            uint32_t char_w = font->glyph[i].x1 - font->glyph[i].x0;
            uint32_t char_h = font->glyph[i].y1 - font->glyph[i].y0;
            glTexSubImage3D(
                GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, char_w, char_h, 1, GL_RED, GL_UNSIGNED_BYTE,
                &font->pixels[font->glyph[i].y0 * font->texture_width + font->glyph[i].x0]);

//            log_gl_error_cmd();
        }

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 1);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//        log_gl_error_cmd();

        // Setting up buffers and attributes
        GLuint vbo;
        GLuint ebo;

        glGenVertexArrays(1, &quad_vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(quad_vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * VERTICES_PER_QUAD, quad_vertices,
                     GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES, GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)nullptr);
        // texture coord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (GLvoid*)(sizeof(math::vec2)));

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

//        log_gl_error_cmd();
    }

    void TextFt2::set_text(const char* txt)
    {
        const size_t chars_num = strlen(txt);
        if(!chars_num) {
            log_warn_cmd("Setting empty string.");
            return;
        }

        lenght = chars_num;
        text = txt;
    }

    void TextFt2::set_pos(const math::vec2& pos, const float rotation)
    {
        if(!lenght) {
            log_warn_cmd("Setting position to an empty text!");
            return;
        }

        uint32_t char_indices[lenght];
        math::mat4 model_matrices[lenght];
        math::vec3 poss[lenght];
        GLuint glyph_vbo;
        GLuint model_vbo;
        size_t i = 1;
        const auto* txt_chars = (const uint8_t*)text.c_str();

        // Setting up position for the first character in text.
        float prev_x = pos.x;
        char_indices[0] = *txt_chars;
        // (Xpos + glyph_advance_x + glyph_bearing_x ; Ypos - glyph_bearing_y + font_face_ascender)
        poss[0] = {pos.x, pos.y - font->glyph[*txt_chars].bearing_y + font->ascender, 0.f};
        model_matrices[0] = math::transform(math::vec3(48, 48, 1.0f), math::vec3(0.f, 0.f, 1.0f),
                                            rotation, poss[0]);

        // Setting up position for the rest characters.
        for(const auto* ch = txt_chars + 1; i < lenght; ++ch, ++i) {
            uint32_t curr_char = *ch;
            uint32_t prev_char = *(ch - 1);
            uint32_t ascender = font->ascender;
            uint32_t advance_x = font->glyph[prev_char].advance_x;
            uint32_t bearing_x = font->glyph[prev_char].bearing_x;
            uint32_t bearing_y = font->glyph[curr_char].bearing_y;

            char_indices[i] = curr_char;
            poss[i] = math::vec3(prev_x + advance_x + bearing_x, pos.y - bearing_y + ascender, 0.f);
            prev_x += advance_x + bearing_x;
            model_matrices[i] = math::transform(math::vec3(font->height, font->height, 1.0f),
                                                math::vec3(0.0f, 0.0f, 1.0f), rotation, poss[i]);
        }

        // setting 'l_glyphid' attribute located in 'glyph_vbo' buffer
        glGenBuffers(1, &glyph_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(uint32_t) * lenght, char_indices, GL_STATIC_DRAW);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(uint32_t), (GLvoid*)nullptr);
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);

        // setting 'l_model' attribute, located in 'model_vbo' buffer
        glGenBuffers(1, &model_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, model_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(math::mat4) * lenght, model_matrices, GL_STATIC_DRAW);

        // FIXME: copied from "sprite_sheet_inst.cpp : 67"
        size_t matrow_size = sizeof(float) * 4;
        for(size_t j = 0; j < lenght; ++j) {
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * matrow_size, (GLvoid*)nullptr);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * matrow_size,
                                  (GLvoid*)(matrow_size));
            glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * matrow_size,
                                  (GLvoid*)(2 * matrow_size));
            glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * matrow_size,
                                  (GLvoid*)(3 * matrow_size));

            glEnableVertexAttribArray(3);
            glEnableVertexAttribArray(4);
            glEnableVertexAttribArray(5);
            glEnableVertexAttribArray(6);

            glVertexAttribDivisor(3, 1);
            glVertexAttribDivisor(4, 1);
            glVertexAttribDivisor(5, 1);
            glVertexAttribDivisor(6, 1);
        }

//        log_gl_error_cmd();
    }

    void TextFt2::draw()
    {
        shader->run();
        glActiveTexture(GL_TEXTURE0 + obj_id);
        glBindTexture(GL_TEXTURE_2D_ARRAY, obj_id);
        glBindVertexArray(quad_vao);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, lenght);
    }
}