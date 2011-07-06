#include "GRbf.h"

#include <math.h>
#include <vector>
#include "fullMatrix.h"
#include "Octree.h"
#include "SPoint3.h"
#include "SVector3.h"
#include "SBoundingBox3d.h"
#include "OS.h"
#include "MVertex.h"
#include "MVertexPositionSet.h"

#if defined(HAVE_ANN)
#include <ANN/ANN.h>
#endif

static void SphereBB(void *a, double*mmin, double*mmax){
  Sphere *t = (Sphere *)a;
  mmin[0] = t->center.x()-t->radius; 
  mmin[1] = t->center.y()-t->radius;
  mmin[2] = t->center.z()-t->radius;
  mmax[0] = t->center.x()+t->radius; 
  mmax[1] = t->center.y()+t->radius; 
  mmax[2] = t->center.z()+t->radius; 
}

static void SphereCentroid(void *a, double*c){
  Sphere *t = (Sphere *)a;
  c[0] = t->center.x();
  c[1] = t->center.y();
  c[2] = t->center.z();
}

static int SphereInEle(void *a, double*c){
  Sphere *t = (Sphere *)a;
  double dist = sqrt((c[0]-t->center.x())*(c[0]-t->center.x())+
		     (c[1]-t->center.y())*(c[1]-t->center.y())+
		     (c[2]-t->center.z())*(c[2]-t->center.z()) );
  if(dist < t->radius){
    return 1;
  }
  return 0;
}
static void printNodes(std::set<MVertex *> myNodes){
  FILE * xyz = fopen("myNodes.pos","w");
  fprintf(xyz,"View \"\"{\n");
 for(std::set<MVertex *>::iterator itv = myNodes.begin(); itv !=myNodes.end() ; ++itv){
   MVertex *v = *itv;
   fprintf(xyz,"SP(%g,%g,%g){%g};\n", v->x(), v->y(), v->z(), 1.0);
 }
 fprintf(xyz,"};\n");
 fclose(xyz);
}

GRbf::GRbf (double sizeBox, int variableEps, int rbfFun, std::map<MVertex*, SVector3> _normals, 
	    std::set<MVertex *> allNodes, std::vector<MVertex*> bcNodes) 
  :  sBox(sizeBox), variableShapeParam(variableEps), radialFunctionIndex (rbfFun),  XYZkdtree(0), _inUV(0)
{

  allCenters.resize(allNodes.size(),3);
  double tol=  5.e-2*sBox;

  //remove duplicate vertices
  //add bc nodes
  for(unsigned int i = 0; i < bcNodes.size(); i++) myNodes.insert(bcNodes[i]);
  //then  create Mvertex position
  std::vector<MVertex*> vertices( allNodes.begin(), allNodes.end() );
  MVertexPositionSet pos(vertices);
  for(unsigned int i = 0; i < vertices.size(); i++){
    MVertex *v = vertices[i];
    pos.find(v->x(), v->y(), v->z(), tol);
    allCenters(i,0) = v->x()/sBox;
    allCenters(i,1) = v->y()/sBox;
    allCenters(i,2) = v->z()/sBox;
    _mapAllV.insert(std::make_pair(v, i));
  }
  //keep only no duplicate vertices
  for(unsigned int i = 0; i < vertices.size(); i++)
    if(vertices[i]->getIndex()) myNodes.insert(vertices[i]);
      
  //initialize with  points 
  nbNodes= myNodes.size();
  centers.resize(nbNodes,3);
  normals.resize(nbNodes,3);
  int index = 0;
  double dist_min = 1.e6;
  for(std::set<MVertex *>::iterator itv = myNodes.begin(); itv !=myNodes.end() ; ++itv){
    MVertex *v1 = *itv;
    centers(index,0) = v1->x()/sBox;
    centers(index,1) = v1->y()/sBox;
    centers(index,2) = v1->z()/sBox;
    std::map<MVertex*, SVector3>::iterator itn = _normals.find(v1);
    normals(index,0) = itn->second.x();
    normals(index,1) = itn->second.y();
    normals(index,2) = itn->second.z();
    _mapV.insert(std::make_pair(v1, index));
    for(std::set<MVertex *>::iterator itv2 = myNodes.begin(); itv2 !=myNodes.end() ; itv2++){
      MVertex *v2 = *itv2;
      double dist = sqrt((v1->x()-v2->x())*(v1->x()-v2->x())+(v1->y()-v2->y())*(v1->y()-v2->y())
			 +(v1->z()-v2->z())*(v1->z()-v2->z()))/sBox;
      if (dist<dist_min && *itv != *itv2) dist_min = dist;
    }
    index++;
  }
 
  delta = 0.33*dist_min;//0.6
  radius= nbNodes/10.;   

  matAInv.resize(nbNodes, nbNodes);
  matAInv = generateRbfMat(0,centers,centers);
  matAInv.invertInPlace();
  isLocal = false;
  extendedX.resize(3*nbNodes,3);

  Msg::Info("*****************************************");
  Msg::Info("*** RBF nbNodes=%d (all=%d) ", myNodes.size(), allNodes.size());
  Msg::Info("*** RBF rad=%g dist_min =%g ", radius, dist_min);
  Msg::Info("*****************************************");  

  printNodes(myNodes);
  //exit(1);

}

GRbf::~GRbf(){
  delete XYZkdtree;
  delete UVkdtree;
  annDeallocPts(XYZnodes);
  annDeallocPts(UVnodes);
}

void GRbf::buildXYZkdtree(){

#if defined (HAVE_ANN)
  XYZnodes = annAllocPts(nbNodes, 3);
  for(int i = 0; i < nbNodes; i++){
    XYZnodes[i][0] = centers(i,0); 
    XYZnodes[i][1] = centers(i,1); 
    XYZnodes[i][2] = centers(i,2); 
  }
  XYZkdtree = new ANNkd_tree(XYZnodes, nbNodes, 3);
#endif
  
}

void GRbf::buildOctree(double radius){

  SBoundingBox3d bb;
  for (int i= 0; i< nbNodes; i++)
    bb += SPoint3(centers(i,0),centers(i,1), centers(i,2));
  double origin[3] = {bb.min().x(), bb.min().y(), bb.min().z()};
  double ssize[3] = {bb.max().x() - bb.min().x(),
                     bb.max().y() - bb.min().y(),
                     bb.max().z() - bb.min().z()};
  const int maxElePerBucket = 10;
  Octree *oct = Octree_Create(maxElePerBucket, origin, ssize, SphereBB,
			      SphereCentroid, SphereInEle);

  Sphere *_sph = new Sphere[nbNodes];
  for (int i= 0; i< nbNodes; i++){
    _sph[i].index = i;
    _sph[i].radius = radius;
    _sph[i].center = SPoint3(centers(i,0),centers(i,1), centers(i,2));
    Octree_Insert(&_sph[i], oct);
  }
  Octree_Arrange(oct);

  for (int i= 0; i< nbNodes; i++){
    std::list<void*> l;
    double P[3] = {centers(i,0),centers(i,1), centers(i,2)};
    Octree_SearchAll(P, oct, &l);
    nodesInSphere[i].push_back(i);
    if (l.size()== 1) printf("*** WARNING: Found only %d sphere ! \n", (int)l.size());
    for (std::list<void*>::iterator it = l.begin(); it != l.end(); it++) {
      Sphere *sph = (Sphere*) *it;
      std::vector<int> &pts = nodesInSphere[i];
      if (sph->index != i)  nodesInSphere[i].push_back(sph->index);
    }
  }

  Octree_Delete(oct);

  buildXYZkdtree();

}

void GRbf::curvature(double radius,
		    const fullMatrix<double> &cntrs,
		    const fullMatrix<double> &node,		
		    fullMatrix<double> &curvature){

//   fullMatrix<double> nodes_in_sph,local_normals,level_set_nodes,level_set_funvals,
//     sx,sy,sz, sxx,syy,szz, sxy,sxz,syz,sLap;

//   isLocal = true;
//   curvature.resize(node.size1(), node.size1());

//   if(nodesInSphere.size() == 0) buildOctree(radius);

//   for (int i = 0;i<node.size1() ; ++i){ 
// #if defined (HAVE_ANN)
//     double xyz[3] = {node(i,0), node(i,1), node(i,2) };
//     XYZkdtree->annkSearch(xyz,  num_neighbours, index, dist);
//     std::vector<int> &pts = nodesInSphere[index[0]];
// #endif

//     fullMatrix<double> nodes_in_sph(pts.size(),3), local_normals(pts.size(),3);
//     fullMatrix<double> LocalOper;
  
//     for (int k=0; k< pts.size(); k++){
//       nodes_in_sph(k,0) = cntrs(pts[k],0);
//       nodes_in_sph(k,1) = cntrs(pts[k],1);
//       nodes_in_sph(k,2) = cntrs(pts[k],2);
//       local_normals(k,0)=normals(pts[k],0);
//       local_normals(k,1)=normals(pts[k],1);
//       local_normals(k,2)=normals(pts[k],2);
//     }

//     setup_level_set(nodes_in_sph,local_normals,level_set_nodes,level_set_funvals);

//     evalRbfDer(1,level_set_nodes,node,level_set_funvals,sx);
//     evalRbfDer(2,level_set_nodes,node,level_set_funvals,sy);
//     evalRbfDer(3,level_set_nodes,node,level_set_funvals,sz);
//     evalRbfDer(11,level_set_nodes,node,level_set_funvals,sxx);
//     evalRbfDer(12,level_set_nodes,node,level_set_funvals,sxy);
//     evalRbfDer(13,level_set_nodes,node,level_set_funvals,sxz);
//     evalRbfDer(22,level_set_nodes,node,level_set_funvals,syy);
//     evalRbfDer(23,level_set_nodes,node,level_set_funvals,syz);
//     evalRbfDer(33,level_set_nodes,node,level_set_funvals,szz);
//     evalRbfDer(222,level_set_nodes,node,level_set_funvals,sLap);

//     double norm_grad_s = sqrt(sx(i,0)*sx(i,0)+sy(i,0)*sy(i,0)+sz(i,0)*sz(i,0));
//     double curv = -sLap(i,0)/norm_grad_s + 
//       (sx(i,0)*sx(i,0)*sxx(i,0)+sy(i,0)*sy(i,0)*syy(i,0)+sz(i,0)*sz(i,0)*szz(i,0)+2*sx(i,0)*sy(i,0)*sxy(i,0)+2*sy(i,0)*sz(i,0)*syz(i,0)+2*sx(i,0)*sz(i,0)*sxz(i,0))/
//       (norm_grad_s*norm_grad_s*norm_grad_s);
//     curvature(i,0) = fabs(curv);
//   }

}


double GRbf::evalRadialFnDer (int p, double dx, double dy, double dz, double ep){
	
  double r2 = dx*dx+dy*dy+dz*dz; //r^2 
  switch (radialFunctionIndex) {
  case 0 : // GA
    switch (p) {
    case 0: return exp(-ep*ep*r2);
    case 1: return -2*ep*ep*dx*exp(-ep*ep*r2); // _x
    case 2: return -2*ep*ep*dy*exp(-ep*ep*r2); // _y
    case 3: return -2*ep*ep*dz*exp(-ep*ep*r2); // _z
    case 11: return -2*ep*ep*(1-2*ep*ep*dx*dx)*exp(-ep*ep*r2); // _xx
    case 12: return 4*ep*ep*ep*ep*dx*dy*exp(-ep*ep*r2); // _xy
    case 13: return 4*ep*ep*ep*ep*dx*dz*exp(-ep*ep*r2); // ...
    case 22: return -2*ep*ep*(1-2*ep*ep*dy*dy)*exp(-ep*ep*r2);
    case 23: return 4*ep*ep*ep*ep*dy*dz*exp(-ep*ep*r2);
    case 33: return -2*ep*ep*(1-2*ep*ep*dz*dz)*exp(-ep*ep*r2);
    case 222: return -2*ep*ep*(3-2*ep*ep*r2)*exp(-ep*ep*r2); //Laplacian
    }
  case 1 : //MQ
    switch (p) {
    case 0: return sqrt(1+ep*ep*r2);
    case 1: return ep*ep*dx/sqrt(1+ep*ep*r2);
    case 2: return ep*ep*dy/sqrt(1+ep*ep*r2);
    case 3: return ep*ep*dz/sqrt(1+ep*ep*r2);
    case 11: return ep*ep*((1-ep*ep*r2)-ep*ep*dx*dx)/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2)); // _xx
    case 12: return -ep*ep*ep*ep*dx*dy/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2)); // _xy
    case 13: return -ep*ep*ep*ep*dx*dz/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2)); // ...
    case 22: return ep*ep*((1-ep*ep*r2)-ep*ep*dy*dy)/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2));
    case 23: return -ep*ep*ep*ep*dy*dz/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2));
    case 33: return ep*ep*((1-ep*ep*r2)-ep*ep*dz*dz)/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2));
    case 222: return ep*ep*(3+ep*ep*2*r2)/sqrt((1+ep*ep*r2)*(1+ep*ep*r2)*(1+ep*ep*r2));
    }
  }	
}

fullMatrix<double> GRbf::generateRbfMat(int p, 
					const fullMatrix<double> &nodes1,
					const fullMatrix<double> &nodes2) {

  int m = nodes2.size1();
  int n = nodes1.size1();
  fullMatrix<double> rbfMat(m,n);

  double eps =0.5/delta; //0.0677*(nbNodes^0.28)/min_dist;
  if (_inUV) eps = 0.4/deltaUV;
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++) {
      double dx = nodes2(i,0)-nodes1(j,0);
      double dy = nodes2(i,1)-nodes1(j,1);
      double dz = nodes2(i,2)-nodes1(j,2);
      rbfMat(i,j) = evalRadialFnDer(p,dx,dy,dz,eps);
    }
  }

  return rbfMat;

}

void GRbf::RbfOp(int p,
		const fullMatrix<double> &cntrs,
		const fullMatrix<double> &nodes, 
		 fullMatrix<double> &D) {

  fullMatrix<double> rbfInvA, rbfMatB; 

  D.resize(nodes.size1(), cntrs.size1());

  if (isLocal){
    rbfInvA = generateRbfMat(0,cntrs,cntrs);
    rbfInvA.invertInPlace();
   }
   else{
     if (cntrs.size1() == nbNodes )
       rbfInvA = matAInv;
     else if (cntrs.size1() == 3*nbNodes )
       rbfInvA  = matAInv_nn;
     else{
       rbfInvA = generateRbfMat(0,cntrs,cntrs);
       rbfInvA.invertInPlace();
     }
 }
 
  rbfMatB = generateRbfMat(p,cntrs,nodes);
  D.gemm(rbfMatB, rbfInvA, 1.0, 0.0);
}

//Evaluates the RBF (p)th derivative w.r.t. the (q)th variable at the new nodes
void GRbf::evalRbfDer(int p, 
		     const fullMatrix<double> &cntrs,
		     const fullMatrix<double> &nodes,
		     const fullMatrix<double> &fValues, 
		      fullMatrix<double> &fApprox) {

  fApprox.resize(nodes.size1(),fValues.size2());
  fullMatrix<double> D;
  RbfOp(p,cntrs,nodes,D);
  fApprox.gemm(D,fValues, 1.0, 0.0);

}

void GRbf::setup_level_set(const fullMatrix<double> &cntrs,
			   const fullMatrix<double> &normals,
			   fullMatrix<double> &level_set_nodes, 
			   fullMatrix<double> &level_set_funvals){

  int numNodes = cntrs.size1();
  int nTot = 3*numNodes;
  double normFactor;
  level_set_nodes.resize(nTot,3);
  level_set_funvals.resize(nTot,1);
  fullMatrix<double> ONES(numNodes+1,1),sx(numNodes,1),sy(numNodes,1),sz(numNodes,1),norms(numNodes,3), cntrsPlus(numNodes+1,3);

  //Computes the normal vectors to the surface at each node
  for (int i=0;i<numNodes ; ++i){
    ONES(i,0)=1.0;
    cntrsPlus(i,0) = cntrs(i,0);
    cntrsPlus(i,1) = cntrs(i,1);
    cntrsPlus(i,2) = cntrs(i,2);
  }
  ONES(numNodes,0) = 5.0;
  cntrsPlus(numNodes,0) = cntrs(0,0)+10.*sBox;
  cntrsPlus(numNodes,1) = cntrs(0,1)+10.*sBox;
  cntrsPlus(numNodes,2) = cntrs(0,2)+10.*sBox;
  
  evalRbfDer(1,cntrsPlus,cntrs,ONES,sx);
  evalRbfDer(2,cntrsPlus,cntrs,ONES,sy);
  evalRbfDer(3,cntrsPlus,cntrs,ONES,sz);
  for (int i=0;i<numNodes ; ++i){
    normFactor = sqrt(sx(i,0)*sx(i,0)+sy(i,0)*sy(i,0)+sz(i,0)*sz(i,0));
    sx(i,0) = sx(i,0)/normFactor;
    sy(i,0) = sy(i,0)/normFactor;
    sz(i,0) = sz(i,0)/normFactor;
    norms(i,0) = sx(i,0);norms(i,1) = sy(i,0);norms(i,2) = sz(i,0);
  }

  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<3 ; ++j){
      level_set_nodes(i,j) = cntrs(i,j);
      level_set_nodes(i+numNodes,j) = cntrs(i,j)-delta*norms(i,j);
      level_set_nodes(i+2*numNodes,j) = cntrs(i,j)+delta*norms(i,j);  
    }
    level_set_funvals(i,0) = 0.0;
    level_set_funvals(i+numNodes,0) = -1;
    level_set_funvals(i+2*numNodes,0) = 1;
  }
  
  matAInv_nn.resize(nTot, nTot);
  matAInv_nn = generateRbfMat(0,level_set_nodes,level_set_nodes);
  matAInv_nn.invertInPlace();

}


void GRbf::RbfLapSurface_local_projection(const fullMatrix<double> &cntrs,
					 const fullMatrix<double> &normals,
					 fullMatrix<double> &Oper) {
  isLocal = true;
  int numNodes = cntrs.size1();
  Oper.resize(numNodes,numNodes);

  if(nodesInSphere.size() == 0) buildOctree(radius);

  for (int i=0;i<numNodes ; ++i){
    std::vector<int> &pts = nodesInSphere[i];

    fullMatrix<double> nodes_in_sph(pts.size(),3), local_normals(pts.size(),3);
    fullMatrix<double> LocalOper;

    LocalOper.setAll(0.0);

    for (int k=0; k< pts.size(); k++){
      nodes_in_sph(k,0) = cntrs(pts[k],0);
      nodes_in_sph(k,1) = cntrs(pts[k],1);
      nodes_in_sph(k,2) = cntrs(pts[k],2);
      local_normals(k,0)=normals(pts[k],0);
      local_normals(k,1)=normals(pts[k],1);
      local_normals(k,2)=normals(pts[k],2);
    }
    
    RbfLapSurface_global_projection(nodes_in_sph,local_normals,LocalOper);

    for (int j=0;j<pts.size() ; j++)
      Oper(i,pts[j])=LocalOper(0,j);   
  }
}

void GRbf::RbfLapSurface_global_projection( const fullMatrix<double> &cntrs,
					   const fullMatrix<double> &normals,
					   fullMatrix<double> &Oper){
 
  int numNodes = cntrs.size1();
  Oper.resize(numNodes,numNodes);

  fullMatrix<double> sx(numNodes,1),sy(numNodes,1),sz(numNodes,1),
    Dx(numNodes,numNodes),Dy(numNodes,numNodes),Dz(numNodes,numNodes),
    PDx(numNodes,numNodes),PDy(numNodes,numNodes),PDz(numNodes,numNodes),
    PDxx(numNodes,numNodes),PDyy(numNodes,numNodes),PDzz(numNodes,numNodes);
	
  surfInterp.resize(numNodes,1);
  surfInterp.setAll(1.0);
  evalRbfDer(1,cntrs,cntrs,surfInterp,sx);
  evalRbfDer(2,cntrs,cntrs,surfInterp,sy);
  evalRbfDer(3,cntrs,cntrs,surfInterp,sz);

  // Normalizes
  double norm;
  for (int i=0;i<numNodes;i++){
    norm = sqrt(sx(i,0)*sx(i,0)+sy(i,0)*sy(i,0)+sz(i,0)*sz(i,0));
    sx(i,0) /= norm;
    sy(i,0) /= norm;
    sz(i,0) /= norm;
  }

  // Finds differentiation matrices
  RbfOp(1,cntrs,cntrs,Dx);
  RbfOp(2,cntrs,cntrs,Dy);
  RbfOp(3,cntrs,cntrs,Dz);
  
  // Fills up the operator matrix
  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<numNodes ; ++j){
      PDx(i,j) = (1-sx(i,0)*sx(i,0))*Dx(i,j)-sx(i,0)*sy(i,0)*Dy(i,j)-sx(i,0)*sz(i,0)*Dz(i,j);
      PDy(i,j) = -sx(i,0)*sy(i,0)*Dx(i,j)+(1-sy(i,0)*sy(i,0))*Dy(i,j)-sy(i,0)*sz(i,0)*Dz(i,j);
      PDz(i,j) =  -sx(i,0)*sz(i,0)*Dx(i,j)-sy(i,0)*sz(i,0)*Dy(i,j)+(1-sz(i,0)*sz(i,0))*Dz(i,j);
    }
  }
  PDx.mult(PDx,PDxx);
  PDy.mult(PDy,PDyy);
  PDz.mult(PDz,PDzz);
  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<numNodes ; ++j){
      Oper(i,j) = PDxx(i,j)+PDyy(i,j)+PDzz(i,j);
    }
  }
}

void GRbf::RbfLapSurface_local_CPM(bool isLow, 
				  const fullMatrix<double> &cntrs,
				  const fullMatrix<double> &normals,
				  fullMatrix<double> &Oper){

  isLocal = true;
  int numNodes = cntrs.size1();
  Oper.resize(3*numNodes,3*numNodes);

  buildOctree(radius);
  setup_level_set(cntrs,normals,extendedX,surfInterp);

  for (int i=0;i<numNodes ; ++i){
    std::vector<int> &pts = nodesInSphere[i];
    int nbp = pts.size();
    fullMatrix<double> nodes_in_sph(nbp,3), local_normals(nbp,3);
    fullMatrix<double> LocalOper;
   
    for (int k=0; k< nbp; k++){
      nodes_in_sph(k,0) = cntrs(pts[k],0);
      nodes_in_sph(k,1) = cntrs(pts[k],1);
      nodes_in_sph(k,2) = cntrs(pts[k],2);
      local_normals(k,0)=normals(pts[k],0);
      local_normals(k,1)=normals(pts[k],1);
      local_normals(k,2)=normals(pts[k],2);
    }

    LocalOper.setAll(0.0);
    if (isLow) RbfLapSurface_global_CPM_low(nodes_in_sph,local_normals,LocalOper);
    else       RbfLapSurface_global_CPM_high(nodes_in_sph,local_normals,LocalOper);

    for (int j = 0; j < nbp ; j++){

      Oper(i,pts[j])=LocalOper(0,j); 
      Oper(i,pts[j]+numNodes)=LocalOper(0,j+nbp); 
      Oper(i,pts[j]+2*numNodes)=LocalOper(0,j+2*nbp);

      Oper(i+numNodes,pts[j])=LocalOper(nbp,j);
      Oper(i+numNodes,pts[j]+numNodes)=LocalOper(nbp,j+nbp);
      Oper(i+numNodes,pts[j]+2*numNodes)=LocalOper(nbp,j+2*nbp);

      Oper(i+2*numNodes,pts[j])=LocalOper(2*nbp,j);
      Oper(i+2*numNodes,pts[j]+numNodes)=LocalOper(2*nbp,j+nbp);
      Oper(i+2*numNodes,pts[j]+2*numNodes)=LocalOper(2*nbp,j+2*nbp);
    }
   
  }
 }

//NEW METHOD #1 CPM GLOBAL HIGH
void GRbf::RbfLapSurface_global_CPM_high(const fullMatrix<double> &cntrs,
					const fullMatrix<double> &normals,
					 fullMatrix<double> &Oper){
  int numNodes = cntrs.size1();
  int nnTot = 3*numNodes;
  Oper.resize(nnTot,nnTot);
 
  fullMatrix<double> sx, sy, sz, sxx, sxy, sxz,syy, syz, szz; 
  fullMatrix<double> A, Ax, Ay, Az, Axx, Axy, Axz, Ayy, Ayz, Azz, Alap, AOper, extX, surf; 

  setup_level_set(cntrs,normals,extX,surf);
  if (!isLocal) extendedX = extX;
  if (!isLocal) surfInterp = surf;

  // Find derivatives of the surface interpolant
  evalRbfDer(1,extX,cntrs,surf,sx);
  evalRbfDer(2,extX,cntrs,surf,sy);
  evalRbfDer(3,extX,cntrs,surf,sz);
  evalRbfDer(11,extX,cntrs,surf,sxx);
  evalRbfDer(12,extX,cntrs,surf,sxy);
  evalRbfDer(13,extX,cntrs,surf,sxz);
  evalRbfDer(22,extX,cntrs,surf,syy);
  evalRbfDer(23,extX,cntrs,surf,syz);
  evalRbfDer(33,extX,cntrs,surf,szz);
	
   // Finds differentiation matrices
  A=generateRbfMat(0,extX,extX);
  Ax=generateRbfMat(1,extX,cntrs);
  Ay=generateRbfMat(2,extX,cntrs);
  Az=generateRbfMat(3,extX,cntrs);
  Axx=generateRbfMat(11,extX,cntrs);
  Axy=generateRbfMat(12,extX,cntrs);
  Axz=generateRbfMat(13,extX,cntrs);
  Ayy=generateRbfMat(22,extX,cntrs);
  Ayz=generateRbfMat(23,extX,cntrs);
  Azz=generateRbfMat(33,extX,cntrs);
  Alap=generateRbfMat(222,extX,cntrs);

  // Fills up the operator matrix
  AOper.resize(nnTot, nnTot);
  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<nnTot ; ++j){
      AOper(i,j) = Alap(i,j);
      AOper(i+numNodes,j)=sx(i,0)*Ax(i,j)+sy(i,0)*Ay(i,j)+sz(i,0)*Az(i,j);
      AOper(i+2*numNodes,j)=sx(i,0)*sx(i,0)*Axx(i,j)+sy(i,0)*sy(i,0)*Ayy(i,j)+sz(i,0)*sz(i,0)*Azz(i,j)+2*sx(i,0)*sy(i,0)*Axy(i,j)+2*sx(i,0)*sz(i,0)*Axz(i,j)+2*sy(i,0)*sz(i,0)*Ayz(i,j)+(sx(i,0)*sxx(i,0)+sy(i,0)*sxy(i,0)+sz(i,0)*sxz(i,0))*Ax(i,j)+(sx(i,0)*sxy(i,0)+sy(i,0)*syy(i,0)+sz(i,0)*syz(i,0))*Ay(i,j)+(sx(i,0)*sxz(i,0)+sy(i,0)*syz(i,0)+sz(i,0)*szz(i,0))*Az(i,j);
    }
  }
  A.invertInPlace();
  Oper.gemm(AOper, A, 1.0, 0.0);
}


//NEW METHOD #2 CPM GLOBAL HIGH
//Produces a nxn differentiation matrix (like the projection method)
//So the local method for this is the local projection method
void GRbf::RbfLapSurface_global_CPM_high_2(const fullMatrix<double> &cntrs,
					const fullMatrix<double> &normals,
					fullMatrix<double> &Oper){

  int numNodes = cntrs.size1();
  int nnTot = 3*numNodes;
  Oper.resize(numNodes,numNodes);

  fullMatrix<double> sx(numNodes,1), sy(numNodes,1), sz(numNodes,1), sxx(numNodes,1), sxy(numNodes,1), sxz(numNodes,1),syy(numNodes,1), syz(numNodes,1), szz(numNodes,1); 
  fullMatrix<double> A(nnTot,nnTot), Ax(numNodes,nnTot), Ay(numNodes,nnTot), Az(numNodes,nnTot), Axx(numNodes,nnTot), Axy(numNodes,nnTot), Axz(numNodes,nnTot), Ayy(numNodes,nnTot), Ayz(numNodes,nnTot), Azz(numNodes,nnTot), Alap(numNodes,nnTot), AOper(nnTot,nnTot), Temp(numNodes,nnTot), extX(nnTot,3), surf(nnTot,1); 

  setup_level_set(cntrs,normals,extX,surf);
  if (!isLocal) extendedX = extX;
  if (!isLocal) surfInterp = surf;

  // Find derivatives of the surface interpolant
  evalRbfDer(1,extX,cntrs,surf,sx);
  evalRbfDer(2,extX,cntrs,surf,sy);
  evalRbfDer(3,extX,cntrs,surf,sz);
  evalRbfDer(11,extX,cntrs,surf,sxx);
  evalRbfDer(12,extX,cntrs,surf,sxy);
  evalRbfDer(13,extX,cntrs,surf,sxz);
  evalRbfDer(22,extX,cntrs,surf,syy);
  evalRbfDer(23,extX,cntrs,surf,syz);
  evalRbfDer(33,extX,cntrs,surf,szz);
	
  // Finds differentiation matrices
  A=generateRbfMat(0,extX,extX);
  Ax=generateRbfMat(1,extX,cntrs);
  Ay=generateRbfMat(2,extX,cntrs);
  Az=generateRbfMat(3,extX,cntrs);
  Axx=generateRbfMat(11,extX,cntrs);
  Axy=generateRbfMat(12,extX,cntrs);
  Axz=generateRbfMat(13,extX,cntrs);
  Ayy=generateRbfMat(22,extX,cntrs);
  Ayz=generateRbfMat(23,extX,cntrs);
  Azz=generateRbfMat(33,extX,cntrs);
  Alap=generateRbfMat(222,extX,cntrs);

  // Fills up the operator matrix
  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<nnTot ; ++j){
      AOper(i,j) = A(i,j);
      AOper(i+numNodes,j)=sx(i,0)*Ax(i,j)+sy(i,0)*Ay(i,j)+sz(i,0)*Az(i,j);
      AOper(i+2*numNodes,j)=sx(i,0)*sx(i,0)*Axx(i,j)+sy(i,0)*sy(i,0)*Ayy(i,j)+sz(i,0)*sz(i,0)*Azz(i,j)+2*sx(i,0)*sy(i,0)*Axy(i,j)+2*sx(i,0)*sz(i,0)*Axz(i,j)+2*sy(i,0)*sz(i,0)*Ayz(i,j)+(sx(i,0)*sxx(i,0)+sy(i,0)*sxy(i,0)+sz(i,0)*sxz(i,0))*Ax(i,j)+(sx(i,0)*sxy(i,0)+sy(i,0)*syy(i,0)+sz(i,0)*syz(i,0))*Ay(i,j)+(sx(i,0)*sxz(i,0)+sy(i,0)*syz(i,0)+sz(i,0)*szz(i,0))*Az(i,j);
    }
  }
  AOper.invertInPlace();
  Alap.mult(AOper,Temp);
  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<numNodes ; ++j){
      Oper(i,j) = Temp(i,j);
    }
  }
}

void GRbf::RbfLapSurface_global_CPM_low(const fullMatrix<double> &cntrs,
				       const fullMatrix<double> &normals,
				       fullMatrix<double> &Oper) {

  int numNodes = cntrs.size1();
  int nnTot = 3*numNodes;
  Oper.resize(nnTot,nnTot);

  fullMatrix<double> sx(nnTot,1),sy(nnTot,1),sz(nnTot,1); 
  fullMatrix<double> PLap(numNodes,nnTot), extX(nnTot,3), surf(nnTot,1);
  fullMatrix<double> norm(nnTot,3), extRBFX(2*numNodes,3), Ix2extX(2*numNodes,nnTot);

  //Stage 1 : The Arbitrary surface
  setup_level_set(cntrs,normals,extX,surf);
  if (!isLocal) extendedX = extX;
  if (!isLocal) surfInterp = surf;

  //Computes the normal vectors to the surface at each node
  evalRbfDer(1,extX,extX,surf,sx);
  evalRbfDer(2,extX,extX,surf,sy);
  evalRbfDer(3,extX,extX,surf,sz);
  for (int i=0;i<nnTot ; ++i){
    double normFactor = sqrt(sx(i,0)*sx(i,0)+sy(i,0)*sy(i,0)+sz(i,0)*sz(i,0));
    sx(i,0) = sx(i,0)/normFactor;
    sy(i,0) = sy(i,0)/normFactor;
    sz(i,0) = sz(i,0)/normFactor;
    norm(i,0) = sx(i,0);norm(i,1) = sy(i,0);norm(i,2) = sz(i,0);
  }
	
  //Computes the inside and outside layers nodes of the surface
  for (int i=0;i<numNodes ; ++i){
    for (int j=0;j<3 ; ++j){
      extRBFX(i,j) = cntrs(i,j)-delta*norm(i,j);
      extRBFX(i+numNodes,j) = cntrs(i,j)+delta*norm(i,j);
    }
  }
	
  //Computes the gradient operators
  RbfOp(0,extX,extRBFX,Ix2extX); //'0' for interpolation
  RbfOp(222,extX,cntrs,PLap); //'222' for the Laplacian

  // Fills up the operator matrix	
  for (int i=0; i < numNodes; i++){
    for (int j=0; j < nnTot ; ++j){
      Oper(i,j) = PLap(i,j);
      double del = (i == j) ? -1.0: 0.0; 
      double pos1 = (i+numNodes == j) ? 1.0: 0.0; 
      double pos2 = (i+2*numNodes == j) ? 1.0: 0.0; 
      Oper(i+numNodes,j) = del + Ix2extX(i,j);//+ pos1; //
      Oper(i+2*numNodes,j) = del + Ix2extX(i+numNodes,j); //pos2; //
    }
  }
  
}

void GRbf::solveHarmonicMap(fullMatrix<double> Oper, 
			    std::vector<MVertex*> bcNodes, 
			    std::vector<double> coords, 
			    std::map<MVertex*, SPoint3> &rbf_param){
  int nb = Oper.size2(); 
  UV.resize(nb,2);

  fullMatrix<double> F(nb,2);
  F.scale(0.0);
  for (int i=0; i < bcNodes.size(); i++){
    // std::set<MVertex *>::iterator itN = myNodes.find(bcNodes[i]);
    // if (itN != myNodes.end()){
	std::map<MVertex*, int>::iterator itm = _mapV.find(bcNodes[i]);
	double theta = 2 * M_PI * coords[i];
	int iFix = itm->second;
	for (int j=0;j<nb ; ++j) Oper(iFix,j) = 0.0;
	Oper(iFix,iFix) = 1.0;
	F(iFix,0) = cos(theta);
	F(iFix,1) = sin(theta);
    //}
  }
  Oper.invertInPlace();
  Oper.mult(F,UV);

  //ANN UVtree
  double dist_min = 1.e6;
#if defined (HAVE_ANN)
  UVnodes = annAllocPts(nbNodes, 3);
  for(int i = 0; i < nbNodes; i++){
    UVnodes[i][0] = UV(i,0); 
    UVnodes[i][1] = UV(i,1); 
    UVnodes[i][2] = 0.0; 
    for(int j = i+1; j < nbNodes; j++){
      double dist = sqrt((UV(i,0)-UV(j,0))*(UV(i,0)-UV(j,0))+
    			 (UV(i,1)-UV(j,1))*(UV(i,1)-UV(j,1)));
      if (dist<dist_min) dist_min = dist;
    }
  }
  deltaUV = 0.6*dist_min; 
  UVkdtree = new ANNkd_tree(UVnodes, nbNodes, 3);
#endif

  //interpolate
  fullMatrix<double> UVall(allCenters.size1(), 3);
  evalRbfDer(0,centers,allCenters,UV,UVall);

  //fill rbf_param
  std::map<MVertex*, int>::iterator itm = _mapAllV.begin();
  for (; itm != _mapAllV.end(); itm++){
    int index = itm->second;
    SPoint3 coords(UVall(index,0), UVall(index,1), 0.0);
    rbf_param.insert(std::make_pair(itm->first,coords));
  }
 
  Msg::Info("*** RBF solved map");

}

bool GRbf::UVStoXYZ(const double  u_eval, const double v_eval,
		    double &XX, double &YY, double &ZZ, 
		    SVector3 &dXdu, SVector3& dXdv, int num_neighbours){

 num_neighbours = 30;
 fullMatrix<double> u_vec(num_neighbours,3), xyz_local(num_neighbours,3); 
 fullMatrix<double> u_vec_eval(1, 3), nodes_eval(1,3), xu(1,3), xv(1,3);
 u_vec_eval(0,0) = u_eval;
 u_vec_eval(0,1) = v_eval;
 u_vec_eval(0,2) = 0.0;

 //printf("uvw=%g %g %g \n", u_eval, v_eval, 0.0);
#if defined (HAVE_ANN)
   double uvw[3] = { u_eval, v_eval, 0.0 };
   ANNidxArray index = new ANNidx[num_neighbours];
   ANNdistArray dist = new ANNdist[num_neighbours]; 
   UVkdtree->annkSearch(uvw, num_neighbours, index, dist);

 double dist_min  = 1.e6;
 for (int i = 0; i < num_neighbours; i++){

    u_vec(i,0) = UV(index[i],0);
    u_vec(i,1) = UV(index[i],1);
    u_vec(i,2) = 0.0;

    xyz_local(i,0) = extendedX(index[i],0);
    xyz_local(i,1) = extendedX(index[i],1);
    xyz_local(i,2) = extendedX(index[i],2);

    for (int j = i+1; j < num_neighbours; j++){
      double dist = sqrt((UV(index[i],0)-UV(index[j],0))*(UV(index[i],0)-UV(index[j],0))+
    			 (UV(index[i],1)-UV(index[j],1))*(UV(index[i],1)-UV(index[j],1)));
      if (dist<dist_min && dist > 1.e-6) dist_min = dist;
    }
 }
 delete [] index;
 delete [] dist;
#endif
  
   _inUV = 1;
   deltaUV = 0.6*dist_min;
   evalRbfDer(0, u_vec, u_vec_eval,xyz_local, nodes_eval);
   evalRbfDer(1, u_vec, u_vec_eval,xyz_local, xu);
   evalRbfDer(2, u_vec, u_vec_eval,xyz_local, xv);
   _inUV= 0; 

   XX = nodes_eval(0,0)*sBox;
   YY = nodes_eval(0,1)*sBox;
   ZZ = nodes_eval(0,2)*sBox;
   dXdu = SVector3(xu(0,0)*sBox, xu(0,1)*sBox, xu(0,2)*sBox);
   dXdv = SVector3(xv(0,0)*sBox, xv(0,1)*sBox, xv(0,2)*sBox);
   
   return true;

}

// bool GRbf::UVStoXYZ(const double  u_eval, const double v_eval,
// 		    double &XX, double &YY, double &ZZ, 
// 		    SVector3 &dXdu, SVector3& dXdv, int num_neighbours){

//   //Finds the U,V,S (in the 0-level set) that are the 'num_neighbours' closest to u_eval and v_eval.
//   //Thus in total, we're working with '3*num_neighbours' nodes
//   //Say that the vector 'index' gives us the indices of the closest points

//   bool converged = true;
//   num_neighbours = 30;

// #if defined (HAVE_ANN)
//   double uvw[3] = { u_eval, v_eval, 0.0 };
//   ANNidxArray index = new ANNidx[num_neighbours];
//   ANNdistArray dist = new ANNdist[num_neighbours]; 
//   UVkdtree->annkSearch(uvw, num_neighbours, index, dist);
//   int N_nbr = 5;//num_neighbours;
//   fullMatrix<double> ux(N_nbr,3), uy(N_nbr,3), uz(N_nbr,3), u_temp(N_nbr,3);
//   fullMatrix<double> nodes_eval(N_nbr,3),temp_nodes_eval(1,3),temp_u_temp(1,3);
//   fullMatrix<double> u_vec(3*num_neighbours,3); 
//   fullMatrix<double> xyz_local(3*num_neighbours,3); 

//   // u_vec = [u v s] : These are the u,v,s that are on the surface *and* outside of it also! 
//   for (int i = 0; i < num_neighbours; i++){

//     u_vec(i,0) = UV(index[i],0);
//     u_vec(i,1) = UV(index[i],1);
//     u_vec(i,2) = 0.0;

//     u_vec(i+num_neighbours,0) = UV(index[i]+nbNodes,0);
//     u_vec(i+num_neighbours,1) = UV(index[i]+nbNodes,1);
//     u_vec(i+num_neighbours,2) = surfInterp(index[i]+nbNodes,0)*deltaUV; 

//     u_vec(i+2*num_neighbours,0) = UV(index[i]+2*nbNodes,0);
//     u_vec(i+2*num_neighbours,1) = UV(index[i]+2*nbNodes,1);
//     u_vec(i+2*num_neighbours,2) = surfInterp(index[i]+2*nbNodes,0)*deltaUV;

//     xyz_local(i,0) = extendedX(index[i],0);
//     xyz_local(i,1) = extendedX(index[i],1);
//     xyz_local(i,2) = extendedX(index[i],2);

//     xyz_local(i+num_neighbours,0) = extendedX(index[i]+nbNodes,0); 
//     xyz_local(i+num_neighbours,1) = extendedX(index[i]+nbNodes,1);
//     xyz_local(i+num_neighbours,2) = extendedX(index[i]+nbNodes,2);

//     xyz_local(i+2*num_neighbours,0) = extendedX(index[i]+2*nbNodes,0);
//     xyz_local(i+2*num_neighbours,1) = extendedX(index[i]+2*nbNodes,1);
//     xyz_local(i+2*num_neighbours,2) = extendedX(index[i]+2*nbNodes,2);

//   }

//   // u_vec_eval = [u_eval v_eval s_eval]
//   fullMatrix<double> u_vec_eval(1, 3),nodes_vec_eval(1,3),u_test_vec_eval(1,3);
//   u_vec_eval(0,0) = u_eval;
//   u_vec_eval(0,1) = v_eval;
//   u_vec_eval(0,2) = 0.0;

//   //we will use a local interpolation to find the corresponding XYZ point to (u_eval,v_eval).
//   _inUV = 1;
//    evalRbfDer(0, u_vec, u_vec_eval,xyz_local, temp_nodes_eval);
//    nodes_eval(0,0) =  temp_nodes_eval(0,0);
//    nodes_eval(0,1) =  temp_nodes_eval(0,1);
//    nodes_eval(0,2) =  temp_nodes_eval(0,2);
//    _inUV= 0;
//    evalRbfDer(0,xyz_local,nodes_eval,u_vec,temp_u_temp);
//    u_temp(0,0) =  temp_u_temp(0,0);
//    u_temp(0,1) =  temp_u_temp(0,1);
//    u_temp(0,2) =  temp_u_temp(0,2);

//   //we use the closest point
//   for (int i_nbr = 1; i_nbr < N_nbr; i_nbr++){
//     nodes_eval(i_nbr,0) = extendedX(index[i_nbr-1],0); 
//     nodes_eval(i_nbr,1) = extendedX(index[i_nbr-1],1); 
//     nodes_eval(i_nbr,2) = extendedX(index[i_nbr-1],2);
//     u_temp(i_nbr,0)  = UV(index[i_nbr-1],0);
//     u_temp(i_nbr,1)  = UV(index[i_nbr-1],1);
//     u_temp(i_nbr,2)  = 0.0;
//   }
//   delete [] index;
//   delete [] dist;
  
//   int incr = 0;
//   double norm = 0.0;
//   double norm_temp = 0.0;
//   double dist_test = 0.0;
//   fullMatrix<double> Jac(3,3),best_Jac(3,3);
//   fullMatrix<double> normDist(N_nbr,1);
//   fullMatrix<double> nodes_eval_temp(N_nbr,3);
//   double norm_treshhold = 8.0;
//   std::vector<fullMatrix<double> >JacV;

//   while(norm < norm_treshhold && incr < 10){
//     // Find the entries of the m Jacobians
//     evalRbfDer(1,xyz_local,nodes_eval,u_vec,ux);
//     evalRbfDer(2,xyz_local,nodes_eval,u_vec,uy);
//     evalRbfDer(3,xyz_local,nodes_eval,u_vec,uz);
    
//     for (int i_nbr = 0; i_nbr < N_nbr; i_nbr++){
//       // Jacobian of the UVS coordinate system w.r.t. XYZ
//       for (int k = 0; k < 3;k++){
// 	Jac(k,0) = ux(i_nbr,k);
// 	Jac(k,1) = uy(i_nbr,k);
// 	Jac(k,2) = uz(i_nbr,k);
//       }    
//       Jac.invertInPlace();
//       JacV.push_back(Jac);
//       for (int j = 0; j< 3;j++)
// 	nodes_eval(i_nbr,j) = nodes_eval(i_nbr,j) 
// 	  + Jac(j,0)*( u_vec_eval(0,0) - u_temp(i_nbr,0)) 
// 	  + Jac(j,1)*( u_vec_eval(0,1) - u_temp(i_nbr,1)) 
// 	  + Jac(j,2)*( 0.0 - u_temp(i_nbr,2)); 
//     } 
//     // Find the corresponding u, v, s
//     evalRbfDer(0,xyz_local,nodes_eval,u_vec,u_temp);
//     norm = 0;
//     for (int i_nbr = 0; i_nbr < N_nbr; i_nbr++){
//       normDist(i_nbr,0) = sqrt((u_temp(i_nbr,0)-u_vec_eval(0,0))*(u_temp(i_nbr,0)-u_vec_eval(0,0))+(u_temp(i_nbr,1)-u_vec_eval(0,1))*(u_temp(i_nbr,1)-u_vec_eval(0,1))+(u_temp(i_nbr,2)*u_temp(i_nbr,2)));
//       norm_temp = -(log10(normDist(i_nbr,0)));
      
//       if (norm_temp>norm_treshhold){
//       	norm = norm_temp;
//        	nodes_vec_eval(0,0)=nodes_eval(i_nbr,0);
//        	nodes_vec_eval(0,1)=nodes_eval(i_nbr,1);
// 	nodes_vec_eval(0,2)=nodes_eval(i_nbr,2);

// 	u_test_vec_eval(0,0)=u_temp(i_nbr,0);
// 	u_test_vec_eval(0,1)=u_temp(i_nbr,1);
// 	u_test_vec_eval(0,2)=u_temp(i_nbr,2);

// 	best_Jac = JacV[i_nbr];
// 	dist_test = normDist(i_nbr,0);
// 	i_nbr=N_nbr; //To get out of the for loof and thus also to get out of the while loop
//       }
//     }
//   incr++;
// }  

// if (norm < norm_treshhold ){
//   printf("Newton not converged (norm=%g, dist=%g) for uv=(%g,%g) UVS=(%g,%g,%g) NB=%d \n", norm, dist_test,u_eval, v_eval, u_test_vec_eval(0,0), u_test_vec_eval(0,1), u_test_vec_eval(0,2), num_neighbours);
//   if (num_neighbours+5 < nbNodes)
//     return UVStoXYZ(u_eval, v_eval, XX, YY,ZZ, dXdu, dXdv, num_neighbours+5);
//   else converged = false;
// }

// XX = nodes_vec_eval(0,0)*sBox;
// YY = nodes_vec_eval(0,1)*sBox;
// ZZ = nodes_vec_eval(0,2)*sBox;
// dXdu = SVector3(best_Jac(0,0)*sBox, best_Jac(1,0)*sBox, best_Jac(2,0)*sBox);
// dXdv = SVector3(best_Jac(0,1)*sBox, best_Jac(1,1)*sBox, best_Jac(2,1)*sBox);

// return converged;

// #endif
// }

