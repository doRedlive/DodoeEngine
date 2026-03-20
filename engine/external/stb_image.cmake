add_library(stb_image STATIC stb_image/stb_image.h stb_image/stb_image_impl.cpp)

target_include_directories(stb_image PUBLIC stb_image)
