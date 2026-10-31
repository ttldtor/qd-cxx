cmake_minimum_required(VERSION 3.25)
# Internal helper. Caller: cmake -DOPENSSL=<exe> -DFIXTURES=<directory> -P GenerateFixtures.cmake.
# Inputs are the pinned OpenSSL executable and an owned output directory. Outputs are
# synthetic test-only CA/server keys and certificates. A failed command is fatal.
if(NOT DEFINED OPENSSL OR NOT EXISTS "${OPENSSL}" OR NOT DEFINED FIXTURES)
    message(FATAL_ERROR "Explicit OPENSSL executable and FIXTURES output directory are required")
endif()
file(MAKE_DIRECTORY "${FIXTURES}")
file(REAL_PATH "${FIXTURES}" FIXTURES)
foreach(name empty.cnf server.ext ca.key ca.pem other-ca.key other-ca.pem server.key server.csr server.pem expired.pem)
    if(EXISTS "${FIXTURES}/${name}")
        message(FATAL_ERROR "Refusing to overwrite existing fixture artifact: ${name}; select a fresh owned directory")
    endif()
endforeach()
file(WRITE "${FIXTURES}/empty.cnf" "")
file(WRITE "${FIXTURES}/server.ext"
    "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost,IP:127.0.0.1\n")
function(openssl)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env "OPENSSL_CONF=${FIXTURES}/empty.cnf"
        "${OPENSSL}" ${ARGV} WORKING_DIRECTORY "${FIXTURES}" RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Synthetic fixture generation failed: ${result}")
    endif()
endfunction()
openssl(req -new -x509 -newkey rsa:2048 -noenc -keyout ca.key -out ca.pem -days 2
    -subj "/CN=QDCXX synthetic CA" -config "${FIXTURES}/empty.cnf"
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign")
openssl(req -new -x509 -newkey rsa:2048 -noenc -keyout other-ca.key -out other-ca.pem -days 2
    -subj "/CN=QDCXX other synthetic CA" -config "${FIXTURES}/empty.cnf"
    -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign")
openssl(req -new -newkey rsa:2048 -noenc -keyout server.key -out server.csr
    -subj "/CN=wrong-common-name.invalid" -config "${FIXTURES}/empty.cnf")
openssl(x509 -req -in server.csr -CA ca.pem -CAkey ca.key -set_serial 11
    -out server.pem -days 2 -extfile server.ext)
openssl(x509 -req -in server.csr -CA ca.pem -CAkey ca.key -set_serial 12
    -out expired.pem -not_before 20200101000000Z -not_after 20200102000000Z -extfile server.ext)
openssl(x509 -in server.pem -noout -dates -ext subjectAltName)
openssl(x509 -in expired.pem -noout -dates)
