// Gmsh - Copyright (C) 1997-2011 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _CURVATUREL_H_
#define _CURVATURE_H_

#include "GModel.h"
#include "STensor3.h"

#include<map>
#include<vector>

class Curvature
{
private:

    //-----------------------------------------
    // TYPEDEFS

    typedef std::vector<GFace*> GFaceList;
    //typedef std::map<int, GEntityList >::iterator GEntityIter;
    typedef std::map<int, GFaceList >::iterator GFaceIter;

    //-----------------------------------------
    // HELPER TYPE FOR WRITING TO VTK FILES
    struct VtkPoint
    {
      double x;
      double y;
      double z;
    };

    //-----------------------------------------
    // MEMBER VARIABLES

    //Static member variable to implement the class curvature as a Singleton
    static Curvature *_instance;
    static bool _destroyed;

    //Boolean to check if the curvature has already been computed
    static bool _alreadyComputedCurvature;

    //Map definition
    std::map<int, int> _VertexToInt;
    std::map<int, int> _ElementToInt;

    //Model and list of selected entities with give physical tag:
    GModel* _model;    

    GFaceList _ptFinalEntityList;

    //Averaged vertex normals
    std::vector<SVector3> _VertexNormal;

    // Vector of principal dircections
    std::vector<SVector3> _pdir1;
    std::vector<SVector3> _pdir2;

    // Vector of principal curvature
    std::vector<double> _curv1;
    std::vector<double> _curv2;
    std::vector<double> _curv12;

    // Area around point
    std::vector<double> _pointareas;
    std::vector<SVector3> _cornerareas;


    //Curvature Tensor per mesh vertex
    std::vector<STensor3> _CurveTensor;

    //Area of a triangle element:
    std::vector<double> _TriangleArea;

    //Area around a mesh vertex:
    std::vector<double> _VertexArea;

    std::vector<double> _VertexCurve;



    //-----------------------------------------
    // PRIVATE METHODS

    //Constructor
    Curvature();

    //Destructor
    ~Curvature();

    static void create();

    static void onDeadReference();


    void initializeMap();
    void computeVertexNormals();
    void curvatureTensor();
    static void rot_coord_sys(const SVector3 &old_u, const SVector3 &old_v,
                              const SVector3 &new_norm, SVector3 &new_u, SVector3 &new_v);
    void proj_curv( const SVector3 &old_u, const SVector3 &old_v, double old_ku, double old_kuv,
                              double old_kv, const SVector3  &new_u, const SVector3 &new_v,
                              double &new_ku, double &new_kuv, double &new_kv);
    void diagonalize_curv(const SVector3 &old_u, const SVector3 &old_v,
                          double ku, double kuv, double kv,
                          const SVector3 &new_norm,
                          SVector3 &pdir1, SVector3 &pdir2, double &k1, double &k2);
    void computePointareas();
    void computeRusinkiewiczNormals();

    // Perform LDL^T decomposition of a symmetric positive definite matrix.
    // Like Cholesky, but no square roots.  Overwrites lower triangle of matrix.

    static inline bool ldltdc(STensor3& A, double rdiag[3])
    {
      double v[2];
      for (int i = 0; i < 3; i++)
      {
        for (int k = 0; k < i; k++)
          v[k] = A(i,k) * rdiag[k];
        for (int j = i; j < 3; j++)
        {
          double sum = A(i,j);
          for (int k = 0; k < i; k++)
            sum -= v[k] * A(j,k);
          if (i == j)
          {
            //if (unlikely(sum <= T(0)))
            if (sum <= 0.0)
              return false;
            rdiag[i] = 1.0 / sum;
          }
          else
          {
            A(j,i) = sum;
          }
        }
      }

      return true;
    }

    // Solve Ax=B after ldltdc
    static inline void ldltsl(STensor3& A, double rdiag[3], double B[3], double x[3])
    {
      int i;
      for (i = 0; i < 3; i++)
      {
        double sum = B[i];
        for (int k = 0; k < i; k++)
          sum -= A(i,k) * x[k];
        x[i] = sum * rdiag[i];
      }
      for (i = 3 - 1; i >= 0; i--)
      {
        double sum = 0;
        for (int k = i + 1; k < 3; k++)
          sum += A(k,i) * x[k];
        x[i] -= sum * rdiag[i];
      }
    }


public:

  //-----------------------------------------
  // PUBLIC METHODS

  static Curvature& getInstance();
  static bool valueAlreadyComputed();
  void setGModel(GModel* model);

  void retrieveCompounds();
  //void retrievePhysicalSurfaces(const std::string & face_tag);

  /// The following function implements algorithm from:
  /// Implementation of an Algorithm for Approximating the Curvature Tensor
  /// on a Triangular Surface Mesh in the Vish Environment
  /// Edwin Matthews, Werner Benger, Marcel Ritter
  void computeCurvature_Simple();

  /// The following function implements algorithm from:
  /// Estimating Curvatures and Their Derivatives on Triangle Meshes
  /// Szymon Rusinkiewicz, Princeton University
  /// Code taken from Rusinkiewicz' 'trimesh2' library
  void computeCurvature_Rusinkiewicz();

  void elementNodalValues(MTriangle* triangle, double& c0, double& c1, double& c2);

  void writeToPosFile( const std::string & filename);

  void writeToVtkFile( const std::string & filename);



};


#endif