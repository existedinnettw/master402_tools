
find_package(EtherCAT REQUIRED)
find_package(argparse REQUIRED CONFIG)
find_package(pugixml REQUIRED CONFIG)
find_package(Boost REQUIRED)

add_executable(pdo_mapping pdo_mapping.cpp)
# target_include_directories(pdo_mapping PUBLIC 
#     # ${CMAKE_CURRENT_SOURCE_DIR}
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include
# )
# target_link_libraries(pdo_mapping PRIVATE master402)
target_link_libraries(pdo_mapping PUBLIC 
    EtherLab::ethercat
    Boost::headers
)
target_link_libraries(pdo_mapping PRIVATE 
    argparse::argparse
    pugixml::pugixml
)
target_compile_features(pdo_mapping PRIVATE cxx_std_17)
