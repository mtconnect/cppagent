cmake_minimum_required(VERSION 2.6)

project(libxml2 C)

# Strip possible quotes because when used quotes are added again.
string(REPLACE "\"" "" LibXML2_SRC_DIR ${LibXML2_SRC_DIR})
file(TO_CMAKE_PATH "${LibXML2_SRC_DIR}" LibXML2_SRC_DIR)

list(APPEND LibXML2_INCLUDE_DIRS "${LibXML2_SRC_DIR}/include")

set(sources 
  "${LibXML2_SRC_DIR}/buf.c"
  "${LibXML2_SRC_DIR}/c14n.c"
  "${LibXML2_SRC_DIR}/catalog.c"
  "${LibXML2_SRC_DIR}/chvalid.c"
  "${LibXML2_SRC_DIR}/dict.c"
  "${LibXML2_SRC_DIR}/encoding.c"
  "${LibXML2_SRC_DIR}/entities.c"
  "${LibXML2_SRC_DIR}/error.c"
  "${LibXML2_SRC_DIR}/globals.c"
  "${LibXML2_SRC_DIR}/hash.c"
  "${LibXML2_SRC_DIR}/HTMLparser.c"
  "${LibXML2_SRC_DIR}/HTMLtree.c"
  "${LibXML2_SRC_DIR}/legacy.c"
  "${LibXML2_SRC_DIR}/list.c"
  "${LibXML2_SRC_DIR}/nanoftp.c"
  "${LibXML2_SRC_DIR}/nanohttp.c"
  "${LibXML2_SRC_DIR}/parser.c"
  "${LibXML2_SRC_DIR}/parserInternals.c"
  "${LibXML2_SRC_DIR}/pattern.c"
  "${LibXML2_SRC_DIR}/relaxng.c"
  "${LibXML2_SRC_DIR}/SAX2.c"
  "${LibXML2_SRC_DIR}/SAX.c"
  "${LibXML2_SRC_DIR}/schematron.c"
  "${LibXML2_SRC_DIR}/threads.c"
  "${LibXML2_SRC_DIR}/tree.c"
  "${LibXML2_SRC_DIR}/uri.c"
  "${LibXML2_SRC_DIR}/valid.c"
  "${LibXML2_SRC_DIR}/xinclude.c"
  "${LibXML2_SRC_DIR}/xlink.c"
  "${LibXML2_SRC_DIR}/xmlIO.c"
  "${LibXML2_SRC_DIR}/xmlmemory.c"
  "${LibXML2_SRC_DIR}/xmlreader.c"
  "${LibXML2_SRC_DIR}/xmlregexp.c"
  "${LibXML2_SRC_DIR}/xmlmodule.c"
  "${LibXML2_SRC_DIR}/xmlsave.c"
  "${LibXML2_SRC_DIR}/xmlschemas.c"
  "${LibXML2_SRC_DIR}/xmlschemastypes.c"
  "${LibXML2_SRC_DIR}/xmlunicode.c"
  "${LibXML2_SRC_DIR}/xmlwriter.c"
  "${LibXML2_SRC_DIR}/xpath.c"
  "${LibXML2_SRC_DIR}/xpointer.c"
  "${LibXML2_SRC_DIR}/xmlstring.c"
)

file(GLOB_RECURSE headers "${LibXML2_SRC_DIR}/include/*.h")

add_library(libxml2 ${sources} ${headers})
target_include_directories(libxml2 PUBLIC ${LibXML2_INCLUDE_DIRS} PRIVATE "${LibXML2_SRC_DIR}")
set_target_properties(libxml2 PROPERTIES PUBLIC_HEADER "${headers}")

install(TARGETS libxml2
LIBRARY DESTINATION lib
PUBLIC_HEADER DESTINATION include/libxml)
