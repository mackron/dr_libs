add_executable(${PROJECT_NAME}-test)
target_link_libraries(${PROJECT_NAME}-test PRIVATE ${PROJECT_NAME})
target_sources(${PROJECT_NAME}-test PRIVATE compile.c)
