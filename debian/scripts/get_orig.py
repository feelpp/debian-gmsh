#!/usr/bin/python
# -*- coding: utf-8 -*-

# The script creates a tar.xz tarball from svn-repository of GMSH
# atool, subversion


import os, sys

os.system('rm -rf gmsh')
print "Cloning the svn-repo."
os.system('svn co https://geuz.org/svn/gmsh/trunk gmsh --username gmsh --password gmsh --non-interactive  --trust-server-cert')


infile = open('gmsh/.svn/entries',"r")
lines = infile.readlines()
revID = int(lines[3])
print "Revision ID is %d" % (revID)

print "Removing svn-files"
os.system('find . -name ".svn" -exec rm -rf {} \;')

print "Making dfsg-compatible"
os.system('rm -f gmsh/contrib/Metis/*.c;\
           rm -f gmsh/contrib/Metis/*.h;\
           rm -f gmsh/contrib/Tetgen1.4/*.cxx; \
           rm -f gmsh/contrib/Tetgen1.4/*.h; \
           rm -f gmsh/contrib/Tetgen1.4/LICENSE;  \
           rm -f gmsh/contrib/Tetgen1.5/*.cxx; \
           rm -f gmsh/contrib/Tetgen1.5/*.h; \
           rm -f gmsh/contrib/Tetgen1.5/*.poly\
           rm -f gmsh/contrib/Tetgen1.5/makefile\
           rm -f gmsh/contrib/Tetgen1.5/LICENSE;')
 
folderName = 'gmsh-2.5.1~beta2~svn%s~dfsg' % (revID)
fileName = 'gmsh_2.5.1~beta2~svn%s~dfsg.orig.tar.xz' % (revID)
#renaming the folder
os.system('mv gmsh %s' % (folderName))
#packing
print "Creating a tarball."
os.system('apack %s %s' % (fileName, folderName))
#Removing folder
os.system(('rm -rf %s')% (folderName))
