CMAKE_MINIMUM_REQUIRED(VERSION 3.15)

PROJECT(UPP_SP2)

SET(CMAKE_CXX_STANDARD 23)

# Set compiler flags for Debug and RelWithDebInfo builds
SET(
	CMAKE_CXX_FLAGS_DEBUG
	"${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -pedantic -g3 -gdwarf-5 -fno-omit-frame-pointer"
)
SET(
	CMAKE_CXX_FLAGS_RELWITHDEBINFO
	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g3 -gdwarf-5 -fno-omit-frame-pointer"
)

FILE(GLOB_RECURSE src src/*.cpp src/*.h)

# nastavime, ze nechceme pouzivat SSL ve vychozim stavu - pokud chcete, muzete
# zapnout, abyste se dostali na https
SET(USE_SSL OFF CACHE BOOL "Use SSL")

FIND_PACKAGE(MPI REQUIRED)

IF(USE_SSL)
	ADD_DEFINITIONS(-DUSE_SSL)
	# najdeme OpenSSL
	FIND_PACKAGE(OpenSSL REQUIRED)

	INCLUDE_DIRECTORIES("${OPENSSL_INCLUDE_DIR}")
ENDIF()

INCLUDE_DIRECTORIES("dep/cpp-httplib")

ADD_EXECUTABLE(UPP-SP2 ${src})

INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
TARGET_LINK_LIBRARIES(UPP-SP2 MPI::MPI_CXX)

# pokud chceme pouzivat SSL, musime prilinkovat OpenSSL
IF(USE_SSL)
	TARGET_LINK_LIBRARIES(UPP-SP2 OpenSSL::SSL OpenSSL::Crypto)
ENDIF()
