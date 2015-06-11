cmake_minimum_required (VERSION 2.6) 
project (sdl2t) 

cmake_policy(SET CMP0037 NEW)

set(CMAKE_CXX_FLAGS "-std=c++11 -g")

add_executable(sdl2t main.cpp AEMatrix4f4.cpp AEMatrix3f3.cpp AEVectorMath.cpp AEMesh.cpp objImport.cpp)
target_link_libraries(sdl2t SDL2 GL)

#add_executable(video_ffmpeg video.cpp AEMatrix4f4.cpp AEMatrix3f3.cpp AEVectorMath.cpp AEMesh.cpp objImport.cpp)
#target_link_libraries(video_ffmpeg SDL2 GL avcodec avutil avformat swscale)

add_executable(geometry geometry.cpp AEMatrix4f4.cpp AEMatrix3f3.cpp AEVectorMath.cpp AEMesh.cpp objImport.cpp)
target_link_libraries(geometry SDL2 GL)
