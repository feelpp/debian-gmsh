#!/bin/sh
# The script creates a tar.xz tarball from svn-repository of GMSH
# atool, subversion


rm -rf gmsh
echo Cloning the svn-repo
svn co https://geuz.org/svn/gmsh/trunk gmsh --username gmsh --password gmsh --non-interactive  --trust-server-cert

REVISION=`svn info gmsh ./ | grep '^Revision:' | sed -e 's/^Revision: //'`
echo Revision ID is ${REVISION}

find . -name .svn -print0 | xargs -0 rm -r

echo Making dfsg-compatible

rm -f gmsh/contrib/Metis/*.c
rm -f gmsh/contrib/Metis/*.h
rm -f gmsh/contrib/Tetgen1.4/*.cxx
rm -f gmsh/contrib/Tetgen1.4/*.h
rm -f gmsh/contrib/Tetgen1.4/LICENSE
rm -f gmsh/contrib/Tetgen1.5/*.cxx
rm -f gmsh/contrib/Tetgen1.5/*.h
rm -f gmsh/contrib/Tetgen1.5/*.poly
rm -f gmsh/contrib/Tetgen1.5/makefile
rm -f gmsh/contrib/Tetgen1.5/LICENSE

folderName=gmsh-2.6.2~beta1~svn${REVISION}~dfsg
fileName=gmsh_2.6.2~beta2~svn${REVISION}~dfsg.orig.tar.xz

mv gmsh ${folderName}
echo Creating a tarball

apack ${fileName} ${folderName}
rm -rf ${folderName}
