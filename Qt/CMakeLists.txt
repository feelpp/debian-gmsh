# Gmsh - Copyright (C) 1997-2014 C. Geuzaine, J.-F. Remacle
#
# See the LICENSE.txt file for license information. Please report all
# bugs and problems to the public mailing list <gmsh@geuz.org>.

set(SRC
  QGui.cpp
  GLWidget.cpp
  graphicWindow.cpp
)

set(MOC_HDR
  GLWidget.h
  graphicWindow.h
)

file(GLOB HDR RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h) 
append_gmsh_src(Qt "${SRC};${HDR}")

foreach(FILE ${MOC_HDR})
  list(APPEND MOC_LIST Qt/${FILE})
endforeach(FILE)
set(GMSH_MOC_HDR ${GMSH_MOC_HDR};${MOC_LIST} PARENT_SCOPE)
