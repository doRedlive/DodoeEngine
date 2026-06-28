// do@Redlive
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

typedef GLADloadproc GLADloadfunc;

#define gladLoadGL gladLoadGLLoader

#define GLAD_GL_VERSION_4_6 1
#define GLAD_GL_ARB_compute_shader 1
#define GLAD_GL_ARB_shader_storage_buffer_object 1
#define GLAD_GL_ARB_texture_storage 1
#define GLAD_GL_ARB_multi_draw_indirect 1
#define GLAD_GL_ARB_indirect_parameters 1
#define GLAD_GL_ARB_base_instance 1
#define GLAD_GL_ARB_copy_image 1
#define GLAD_GL_ARB_shader_image_load_store 1
#define GLAD_GL_KHR_debug 1
#define GLAD_GL_ARB_timer_query 1

#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT   0x83F1
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT   0x83F2
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT   0x83F3
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE

typedef void (APIENTRYP PFN_glMultiDrawElementsIndirectCount)(GLenum, GLenum, const void*, GLintptr, GLsizei, GLsizei);
inline void glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    static PFN_glMultiDrawElementsIndirectCount fn = (PFN_glMultiDrawElementsIndirectCount)glfwGetProcAddress("glMultiDrawElementsIndirectCount");
    if (fn) fn(mode, type, indirect, drawcount, maxdrawcount, stride);
}
