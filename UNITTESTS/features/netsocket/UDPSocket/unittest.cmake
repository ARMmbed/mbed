
####################
# UNIT TESTS
####################

set(unittest-sources
  ../features/netsocket/SocketAddress.cpp
  ../features/netsocket/NetworkStack.cpp
  ../features/netsocket/InternetSocket.cpp
  ../features/netsocket/InternetDatagramSocket.cpp
  ../features/netsocket/UDPSocket.cpp
  ../connectivity/libraries/nanostack-libservice/source/libip4string/ip4tos.c
  ../connectivity/libraries/nanostack-libservice/source/libip6string/ip6tos.c
  ../connectivity/libraries/nanostack-libservice/source/libip4string/stoip4.c
  ../connectivity/libraries/nanostack-libservice/source/libip6string/stoip6.c
  ../connectivity/libraries/nanostack-libservice/source/libBits/common_functions.c
)

set(unittest-test-sources
  features/netsocket/UDPSocket/test_UDPSocket.cpp
  stubs/Mutex_stub.cpp
  stubs/mbed_assert_stub.cpp
  stubs/mbed_atomic_stub.c
  stubs/mbed_critical_stub.c
  stubs/equeue_stub.c
  stubs/EventQueue_stub.cpp
  stubs/mbed_error.c
  stubs/mbed_shared_queues_stub.cpp
  stubs/EventFlags_stub.cpp
  stubs/nsapi_dns_stub.cpp
  stubs/stoip4_stub.c
  stubs/ip4tos_stub.c
  stubs/SocketStats_Stub.cpp
)