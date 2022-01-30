add_library(ExampleContract MODULE ExampleContract.cpp ExampleContract.hpp)
target_link_libraries(ExampleContract ${PEERPLAYS_LIBS} ${Boost_LIBRARIES})
