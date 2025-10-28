cmake_minimum_required(VERSION 3.16.0)

project(lemon_core_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找 Qt 和其他依赖
find_package(Qt6 COMPONENTS Test Core Gui Widgets Network REQUIRED)
# 或者使用 Qt5（基于主项目配置）
# find_package(Qt5 COMPONENTS Test Core Gui Widgets Network REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# 启用测试
enable_testing(true)

# 添加包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/base
    ${CMAKE_SOURCE_DIR}/src/core
)

# 为每个测试类创建独立的可执行文件
set(TEST_CLASSES
    Contest
    Contestant
    Task
    TestCase
    JudgingController
)

foreach(TEST_CLASS ${TEST_CLASSES})
    string(TOLOWER ${TEST_CLASS} TEST_CLASS_LOWER)
    set(TEST_TARGET "test_${TEST_CLASS_LOWER}")

    qt_add_executable(${TEST_TARGET} test_${TEST_CLASS_LOWER}.cpp)

    target_link_libraries(${TEST_TARGET}
        PRIVATE
            Qt::Test
            Qt::Core
            Qt::Gui
            Qt::Widgets
            Qt::Network
            lemon-core
            lemon-base
            spdlog::spdlog
            SingleApplication::SingleApplication
    )

    add_dependencies(${TEST_TARGET} lemon-core lemon-base)

    # 添加测试数据
    add_custom_command(TARGET ${TEST_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TEST_TARGET}>/testdata"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/testdata/valid_contest.json
            ${CMAKE_CURRENT_SOURCE_DIR}/testdata/invalid_contest.json
            "$<TARGET_FILE_DIR:${TEST_TARGET}>/testdata"
    )

    # 添加测试用例
    add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})

    set_tests_properties(${TEST_TARGET} PROPERTIES
        LABELS "unit;${TEST_CLASS_LOWER}"
        TIMEOUT 60
    )
endforeach()

# 设置测试属性
set_tests_properties(test_contest PROPERTIES
    LABELS "unit;contest"
    TIMEOUT 60
)

set_tests_properties(test_contestant PROPERTIES
    LABELS "unit;contestant"
    TIMEOUT 60
)

set_tests_properties(test_task PROPERTIES
    LABELS "unit;task"
    TIMEOUT 60
)

set_tests_properties(test_testcase PROPERTIES
    LABELS "unit;testcase"
    TIMEOUT 60
)

set_tests_properties(test_judgingcontroller PROPERTIES
    LABELS "unit;judgingcontroller"
    TIMEOUT 60
)
