SET(CoinUtils_VERSION 2.10.13)
SET(Osi_VERSION 0.107.8)
SET(Clp_VERSION 1.16.10)
SET(Cgl_VERSION 0.59.9)

SET(CoinUtils_URL https://github.com/coin-or/CoinUtils.git)
SET(Osi_URL https://github.com/coin-or/Osi.git)
SET(Clp_URL https://github.com/coin-or/Clp.git)
SET(Cgl_URL https://github.com/coin-or/Cgl.git)
SET(Cbc_URL https://github.com/coin-or/Cbc.git)

FOREACH(COIN_PROJECT CoinUtils Osi Clp Cgl Cbc)
    IF(WIN32)
        SET(${COIN_PROJECT}_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/install/lib/lib${COIN_PROJECT}.lib)
        SET(${COIN_PROJECT}_ADDITIONAL_CMAKE_OPTIONS --enable-msvc)
    ELSE()
        SET(${COIN_PROJECT}_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/install/lib/lib${COIN_PROJECT}.a)
    ENDIF()

    SET(${COIN_PROJECT}_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/install/include)

    ExternalProject_Add(${COIN_PROJECT}
            PREFIX ${COIN_PROJECT}
            GIT_REPOSITORY ${${COIN_PROJECT}_URL}
            GIT_TAG "releases/${${COIN_PROJECT}_VERSION}"
            DOWNLOAD_DIR "${DOWNLOAD_LOCATION}"
            BUILD_IN_SOURCE 1
            CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${COIN_PROJECT}/src/${COIN_PROJECT}/configure --enable-static --prefix=${CMAKE_CURRENT_BINARY_DIR}/install CXXFLAGS=${CMAKE_CXX_FLAGS})
ENDFOREACH()

FOREACH(COIN_PROJECT CoinUtils Osi Clp Cgl)
    LIST(APPEND Cbc_LIBRARIES ${${COIN_PROJECT}_LIBRARIES})
ENDFOREACH()

ADD_DEPENDENCIES(Osi CoinUtils)
ADD_DEPENDENCIES(Clp CoinUtils Osi)
ADD_DEPENDENCIES(Cgl CoinUtils Osi Clp)
ADD_DEPENDENCIES(Cbc CoinUtils Osi Clp Cgl)

LIST(APPEND ${PROJECT_NAME}externalTargets Cbc)