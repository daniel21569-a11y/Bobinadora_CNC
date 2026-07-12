cmake_minimum_required(VERSION 3.16)

project(bobinadora_transformer_sim LANGUAGES CXX)

add_executable(transformer_sim
  src/main.cpp
  ../core/src/transformer_core.cpp
)

target_include_directories(transformer_sim PRIVATE
  ../core/include
)

target_compile_features(transformer_sim PRIVATE cxx_std_17)
