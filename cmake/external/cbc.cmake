FOREACH(COIN_PROJECT CoinUtils Osi Clp Cgl Cbc)
    SET(${COIN_PROJECT}_URL https://github.com/coin-or/${COIN_PROJECT}.git)
    SET(${COIN_PROJECT}_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/install/include)

    ExternalProject_Add(${COIN_PROJECT}
            PREFIX ${COIN_PROJECT}
            GIT_REPOSITORY ${${COIN_PROJECT}_URL}
            GIT_TAG "releases/${${COIN_PROJECT}_VERSION}"
            DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
            BUILD_IN_SOURCE 1
            CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${COIN_PROJECT}/src/${COIN_PROJECT}/configure --with-pic --enable-static --prefix=${CMAKE_CURRENT_BINARY_DIR}/install CXXFLAGS=${CMAKE_CXX_FLAGS})
    LIST(APPEND Cbc_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/install/lib/lib${COIN_PROJECT}.a)
ENDFOREACH()

ADD_DEPENDENCIES(Osi CoinUtils)
ADD_DEPENDENCIES(Clp CoinUtils Osi)
ADD_DEPENDENCIES(Cgl CoinUtils Osi Clp)
ADD_DEPENDENCIES(Cbc CoinUtils Osi Clp Cgl)

LIST(APPEND ${PROJECT_NAME}externalTargets Cbc)