add_library(nesjoy INTERFACE)

target_sources(nesjoy INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/nesjoy.c
	${CMAKE_CURRENT_LIST_DIR}/nesjoy.h
)

target_link_libraries(nesjoy INTERFACE hardware_pio hardware_clocks)

target_include_directories(nesjoy INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)


#pico_generate_pio_header(nesjoy 
#  ${CMAKE_CURRENT_LIST_DIR}/nesjoy_mrmltr.pio
#)
