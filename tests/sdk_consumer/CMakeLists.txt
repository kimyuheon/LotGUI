cmake_minimum_required(VERSION 3.20)
project(LotUISdkConsumer LANGUAGES CXX)

find_package(LotUI CONFIG REQUIRED)

add_executable(lotui_sdk_consumer main.cpp)
target_compile_features(lotui_sdk_consumer PRIVATE cxx_std_17)
target_link_libraries(lotui_sdk_consumer PRIVATE
    LotUI::Widgets LotUI::VulkanEmbedded)

enable_testing()
add_test(NAME lotui_sdk_consumer COMMAND lotui_sdk_consumer)
