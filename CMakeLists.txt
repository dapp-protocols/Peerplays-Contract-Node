project(PeerplaysContractNode)
cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)

find_package(Threads)
find_package(Boost REQUIRED system filesystem serialization chrono date_time coroutine context thread)

set(PEERPLAYS_PATH "/opt/peerplays" CACHE STRING "Path to the Peerplays installation")
set(CMAKE_INSTALL_RPATH "${PEERPLAYS_PATH}/lib")

# Set default install path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PEERPLAYS_PATH}" CACHE PATH "Install prefix" FORCE)
endif()

if (NOT EXISTS "${PEERPLAYS_PATH}/lib/libgraphene_chain.so")
    if (EXISTS "${PEERPLAYS_PATH}/lib/libgraphene_chain.a")
        message(FATAL_ERROR "Peerplays found, but does not have dynamic libraries. Rebuild Peerplays with dynamic libraries and try again.")
    else()
        message(FATAL_ERROR "Peerplays not found. Please set PEERPLAYS_PATH to the directory Peerplays is installed to.")
    endif()
endif()

find_library(FC_LIB REQUIRED NAMES fc fc_debug HINTS "${PEERPLAYS_PATH}/lib")

include_directories("${CMAKE_SOURCE_DIR}" "${PEERPLAYS_PATH}/include")
link_directories("${PEERPLAYS_PATH}/lib" "${PEERPLAYS_PATH}/lib/cryptonomex")

set(PEERPLAYS_LIBS graphene_chain graphene_net graphene_db graphene_protocol graphene_utilities
    ${FC_LIB} secp256k1 z Threads::Threads ssl crypto curl ${CMAKE_DL_LIBS})

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ContractApi" "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(ExampleContract)

file(GLOB_RECURSE Modules Modules/*)
file(GLOB_RECURSE Infra Infra/*)

add_executable(ContractNode main.cpp ${Modules} ContractApi/ContractApi.hpp ${Infra})
target_link_libraries(ContractNode PRIVATE ${PEERPLAYS_LIBS} ${Boost_LIBRARIES})

install(TARGETS ContractNode)
