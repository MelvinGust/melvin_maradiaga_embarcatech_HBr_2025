# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Flag para controlar qual código vai ser compilado.
option(TEST_BUILD "OFF: Compila o .exe para o RP2040. ON: Compila no meu PC o .exe de teste unitario." ON)

if(NOT TEST_BUILD)
    # -------------------- PROD --------------------
    # Aqui temos a CMakeLists original que gera o código que vai ser carregado no RP2040.

    set(PICO_BOARD pico_w CACHE STRING "Board type")
    include(pico_sdk_import.cmake) # Pull in Raspberry Pi Pico SDK (must be before project)
    project(unit_testing C CXX ASM)
    pico_sdk_init() # Initialise the Raspberry Pi Pico SDK


    # Add executable. Default name is the project name, version 0.1
    add_executable(unit_testing
    app/main.c
    inc/temp_sensor.c
    )

    pico_set_program_name(unit_testing "unit_testing")
    pico_set_program_version(unit_testing "0.1")

    # Modify the below lines to enable/disable output over UART/USB
    pico_enable_stdio_uart(unit_testing 0)
    pico_enable_stdio_usb(unit_testing 1)

    # Add the standard library to the build
    target_link_libraries(unit_testing
        pico_stdlib
    )

    target_include_directories(unit_testing PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/inc
    )

    # Add any user requested libraries
    target_link_libraries(unit_testing 
    hardware_adc
    )

    pico_add_extra_outputs(unit_testing)

else()
    # -------------------- TESTE --------------------
    # Aqui temos uma nova CMakeLists, que gera meu executável que vai rodar Unity.
    project(unit_testing_tests C)

    include(CTest)
    add_subdirectory(external)

    # Create test executable
    add_executable(test1_app
        test/test1.c
        test/test_inc/adc_test.c
    )   

    target_link_libraries(test1_app
    Unity
    )

    add_test(test1_test test1_app)
endif()
