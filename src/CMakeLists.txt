project(dgoods)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( dgoods dgoods dgoods.cpp )
target_include_directories( dgoods PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( dgoods ${CMAKE_SOURCE_DIR}/../ricardian )