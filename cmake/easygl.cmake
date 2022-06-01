cmake_minimum_required(VERSION 2.8...3.22) 

project(easygl)



###   VARIABLES   ##############################################################
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -Wno-reorder") #we need c++17 because this solves alignment issues with eigen http://eigen.tuxfamily.org/bz/show_bug.cgi?id=1409
set(CMAKE_CXX_STANDARD 17) #we need c++17 because this solves alignment issues with eigen http://eigen.tuxfamily.org/bz/show_bug.cgi?id=1409
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(EasyGL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
message("EasyGL_ROOT" ${EasyGL_ROOT})



####   GLOBAL OPTIONS   ###https://stackoverflow.com/questions/15201064/cmake-conditional-preprocessor-define-on-code


######   PACKAGES   ############################################################
find_package(GLFW REQUIRED)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs )
#try to compile with pytorch if you can
# get and append paths for finding dep
execute_process( #do it like this https://github.com/facebookresearch/hanabi_SAD/blob/6e4ed590f5912fcb99633f4c224778a3ba78879b/rela/CMakeLists.txt#L10
  COMMAND python -c "import torch; import os; print(os.path.dirname(torch.__file__), end='')"
  OUTPUT_VARIABLE TorchPath
)
#sometimes we want to use libtorch because the pytorch one has a different abi
# set(TorchPath "/home/rosu/work/ws/libtorch-cxx11-abi-shared-with-deps-1.9.0+cu111/libtorch")
# set(TorchPath "/home/rosu/work/ws/libtorch-cxx11-abi-shared-with-deps-1.9.1+cu111/libtorch")
# list(APPEND CMAKE_PREFIX_PATH ${TorchPath})
# find_package(Torch)
# set(TORCH_FOUND False)
if(TorchPath STREQUAL "")
    set(TORCH_FOUND False)
else()
    list(APPEND CMAKE_PREFIX_PATH ${TorchPath})
    find_package(Torch)
endif()


if(NOT TARGET torch)
    message("----------------------easygl no target torch")
else()
    message("----------------------easygl we have target torch")
endif()






###   SOURCES   #################################################################
set(MY_SRC
    ${EasyGL_ROOT}/src/Buf.cxx
    ${EasyGL_ROOT}/src/CubeMap.cxx
    ${EasyGL_ROOT}/src/GBuffer.cxx
    ${EasyGL_ROOT}/src/Shader.cxx
    ${EasyGL_ROOT}/src/Texture2D.cxx
    ${EasyGL_ROOT}/src/VertexArrayObject.cxx
)




###   MAIN LIB  ####################
add_library( easygl_cpp SHARED ${MY_SRC}  ${CMAKE_SOURCE_DIR}/extern/glad/glad.c  )


###   INCLUDES   #########################################################
set(PROJECT_INCLUDE_DIR ${EasyGL_ROOT}/include
                        ${CMAKE_SOURCE_DIR}/extern
                        ${CMAKE_SOURCE_DIR}/deps/loguru
                        ) # Header folder
target_include_directories(easygl_cpp PUBLIC ${PROJECT_INCLUDE_DIR} )
target_include_directories(easygl_cpp PUBLIC ${GLFW_INCLUDE_DIR})
# include_directories(${Boost_INCLUDE_DIR})
target_include_directories(easygl_cpp PUBLIC ${EIGEN3_INCLUDE_DIR})
target_include_directories(easygl_cpp PUBLIC ${OpenCV_INCLUDE_DIRS})
if(${TORCH_FOUND})
    target_include_directories(easygl_cpp PUBLIC ${TORCH_INCLUDE_DIRS})
endif()







##pybind
# pybind11_add_module(easypbr ${PROJECT_SOURCE_DIR}/src/PyBridge.cxx )


###   EXECUTABLE   #######################################
# add_executable(run_easypbr ${PROJECT_SOURCE_DIR}/src/main.cxx  )



###   SET ALL THE GLOBAL OPTIONS #########################################
if(${TORCH_FOUND})
    message("USING TORCH")
    target_compile_definitions(easygl_cpp PUBLIC EASYPBR_WITH_TORCH)
else()
    message("NOT USING TORCH")
endif()

#definitions for cmake variables that are necesarry during runtime
# target_compile_definitions(easygl_cpp PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}") #point to the cmakelist folder of the easy_pbr







###   LIBS   ###############################################
set(LIBS -lpthread -ldl) #because loguru needs them
if(${TORCH_FOUND})
    # message("Torch libraries are ", ${TORCH_LIBRARIES})
    set(LIBS ${LIBS} ${TORCH_LIBRARIES} )
    #torch 1.5.0 and above mess with pybind and we therefore need to link against libtorch_python.so also
    find_library(TORCH_PYTHON_LIBRARY torch_python PATHS "${TORCH_INSTALL_PREFIX}/lib")
    message(STATUS "TORCH_PYTHON_LIBRARY: ${TORCH_PYTHON_LIBRARY}")
    if(TORCH_PYTHON_LIBRARY)
        message(STATUS "Linking to torch_python_library")
        set(LIBS ${LIBS} ${TORCH_PYTHON_LIBRARY} )
    endif()
endif()
# set(LIBS ${LIBS} Eigen3::Eigen  ${Boost_LIBRARIES}  igl::core   ${GLFW_LIBRARIES} ${OpenCV_LIBS} ${PCL_LIBRARIES}  )
set(LIBS ${LIBS} Eigen3::Eigen ${GLFW_LIBRARIES} ${OpenCV_LIBS}  )


target_link_libraries(easygl_cpp PUBLIC ${LIBS} )



export(PACKAGE easygl)

