// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _MODULUS_PHASE_H_
#define _MODULUS_PHASE_H_

#include "Plugin.h"

extern "C"
{
  GMSH_Plugin *GMSH_RegisterModulusPhasePlugin();
}

class GMSH_ModulusPhasePlugin : public GMSH_Post_Plugin
{
public:
  GMSH_ModulusPhasePlugin(){}
  void getName(char *name) const;
  void getInfos(char *author, char *copyright, char *help_text) const;
  void catchErrorMessage(char *errorMessage) const;
  int getNbOptions() const;
  StringXNumber *getOption(int iopt);  
  PView *execute(PView *);
};

#endif