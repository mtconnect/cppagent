####################################################################################################
#             Providing LibXML2 library - https://gitlab.gnome.org/GNOME/libxml2.git               #
#                                                                                                  #
# To use the LibXML2 library simply link to it:                                                    #
# target_link_libraries(<project> PRIVATE LibXml2::LibXml2).                                       #
# This automatically sets all required information such as include directories, definitions etc.   #
####################################################################################################

find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)
