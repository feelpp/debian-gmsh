#include "meshMetric.h"
#include "meshGFaceOptimize.h"
#include "Context.h"
#include "GEntity.h"
#include "GModel.h"
#include "gmshLevelset.h"

static void increaseStencil(MVertex *v, v2t_cont &adj, std::vector<MElement*> &lt){
  std::set<MElement*> stencil;
  std::set<MVertex*> vs;
  stencil.insert(lt.begin(),lt.end());
  for (int i=0;i<lt.size();i++){
    for (int j=0;j<lt[i]->getNumVertices();j++){
      MVertex *v1 = lt[i]->getVertex(j);
      if (v1 != v){
	std::vector<MElement*> &lt2 = adj[v1];
	stencil.insert(lt2.begin(),lt2.end());
      }
    }
  }
  lt.clear();
  lt.insert(lt.begin(),stencil.begin(),stencil.end());
}

meshMetric::meshMetric ( std::vector<MElement*> &elements, simpleFunction<double> *fct,  meshMetric::MetricComputationTechnique technique, 
			 double lcmin, double lcmax,
			 double E, double eps)
  : _technique(technique), _epsilon(eps), _E(E), _fct(fct), hmax(lcmax),hmin(lcmin) 
{
  _dim = elements[0]->getDim();
  computeMetric(elements);
  printMetric("toto.pos");
}


void meshMetric::computeValues( v2t_cont adj){

  v2t_cont :: iterator it = adj.begin(); 
  while (it != adj.end()) {
    std::vector<MElement*> lt = it->second;      
    MVertex *ver = it->first;
    vals[ver]= (*_fct)(ver->x(),ver->y(),ver->z());
    it++;
  }
}

// u = a + b(x-x_0) + c(y-y_0) + d(z-z_0) 
void meshMetric::computeHessian( v2t_cont adj){

  int DIM = _dim + 1;
  for (int ITER=0;ITER<DIM;ITER++){  
    v2t_cont :: iterator it = adj.begin();
    while (it != adj.end()) {
      std::vector<MElement*> lt = it->second;      
      MVertex *ver = it->first;
      while (lt.size() < 7) increaseStencil(ver,adj,lt);
      if ( ver->onWhat()->dim() < _dim ){
	while (lt.size() < 12){
	  increaseStencil(ver,adj,lt);
	}
      }
      
      fullMatrix<double> A  (lt.size(),DIM);
      fullMatrix<double> AT (DIM,lt.size());
      fullMatrix<double> ATA (DIM,DIM);
      fullVector<double> b   (lt.size());
      fullVector<double> ATb (DIM);
      fullVector<double> result (DIM);
      fullMatrix<double> f (1,1);
      for(int i = 0; i < lt.size(); i++) {
	MElement *e = lt[i];
	int npts; IntPt *pts;
	SPoint3 p;      
	e->getIntegrationPoints(0,&npts,&pts);
	if (ITER == 0){ 
	  SPoint3 p;
	  e->pnt(pts[0].pt[0],pts[0].pt[1],pts[0].pt[2],p);
	  b(i) = (*_fct)(p.x(),p.y(),p.z());
	}
	else {
	  double value = 0.0;
	  for (int j=0;j<e->getNumVertices();j++){
	    SVector3 gr = grads[e->getVertex(j)];
	    value += gr(ITER-1);
	  }
	  b(i) = value/(double)e->getNumVertices();
	}
	e->pnt(pts[0].pt[0],pts[0].pt[1],pts[0].pt[2],p);
	A(i,0) = 1.0;
	A(i,1) = p.x() - ver->x();
	A(i,2) = p.y() - ver->y();
	if (_dim == 3) A(i,3) = p.z() - ver->z();
      }
      AT = A.transpose();
      AT.mult(A,ATA);
      AT.mult(b,ATb);
      ATA.luSolve(ATb,result);
      if (ITER == 0)
	grads[ver] = SVector3(result(1),result(2),_dim==2 ? 0.0:result(3));
      else
	dgrads[ITER-1][ver] = SVector3(result(1),result(2),_dim==2? 0.0:result(3));
      ++it;
    }
    if (_technique != meshMetric::HESSIAN)break;
  }
   
}
void meshMetric::computeMetric(std::vector<MElement*> &e){
  
  v2t_cont adj;
  buildVertexToElement (e,adj);

  printf("%d elements are considered in the metric \n",e.size());

  computeValues(adj);
  computeHessian(adj);

  v2t_cont :: iterator it = adj.begin();
  while (it != adj.end()) {
     MVertex *ver = it->first;
  
     SMetric3 H;
     SVector3 gradudx = dgrads[0][ver];
     SVector3 gradudy = dgrads[1][ver];
     SVector3 gradudz = dgrads[2][ver];
     fullMatrix<double> hessian(3,3);
     hessian(0,0) = gradudx(0);
     hessian(1,1) = gradudy(1);
     hessian(2,2) = gradudz(2);
     hessian(1,0) = hessian(0,1) = 0.5 * (gradudx(1) +gradudy(0));
     hessian(2,0) = hessian(0,2) = 0.5 * (gradudx(2) +gradudz(0));
     hessian(2,1) = hessian(1,2) = 0.5 * (gradudy(2) +gradudz(1));

     if (_technique == meshMetric::HESSIAN){
      H.setMat(hessian);
    }
    else if (_technique == meshMetric::FREY){
      SVector3 gr = grads[ver];
      fullMatrix<double> hfrey(hessian);
      hfrey(0,0) +=  gr(0)*gr(0)/(hmin*hmin);
      hfrey(1,1) +=  gr(1)*gr(1)/(hmin*hmin);
      hfrey(2,2) +=  gr(2)*gr(2)/(hmin*hmin);
      hfrey(1,0) +=  gr(1)*gr(0)/(hmin*hmin);
      hfrey(2,0) +=  gr(2)*gr(0)/(hmin*hmin);
      hfrey(2,1) +=  gr(2)*gr(1)/(hmin*hmin);
      hfrey(0,1) = hfrey(1,0) ;
      hfrey(0,2) = hfrey(2,0);
      hfrey(1,2) = hfrey(2,1); 
      H.setMat(hfrey);
    }
    else if (_technique == meshMetric::LEVELSET){
      double dist = fabs(vals[ver]);
      SVector3 gr = grads[ver];
      fullMatrix<double> hcoupez(3,3);
      hcoupez(0,0) = 1./(hmax*hmax);
      hcoupez(1,1) = 1./(hmax*hmax);
      hcoupez(2,2) = 1./(hmax*hmax);
      double norm = sqrt(gr(0)*gr(0)+gr(1)*gr(1)+gr(2)*gr(2));
      if (dist < _E && norm != 0.0){
	double C = 1./(hmin*hmin) -1./(hmax*hmax);
	hcoupez(0,0) += C*gr(0)*gr(0)/norm ;
	hcoupez(1,1) += C*gr(1)*gr(1)/norm ;
	hcoupez(2,2) += C*gr(2)*gr(2)/norm ;
	hcoupez(1,0) = hcoupez(0,1) = C*gr(1)*gr(0)/norm;
	hcoupez(2,0) = hcoupez(0,2) = C*gr(2)*gr(0)/norm;
	hcoupez(2,1) = hcoupez(1,2) = C*gr(2)*gr(1)/norm;
      }
      else if (dist > _E && dist < 2.*_E  && norm != 0.0){
	double hmid = (hmax-hmin)/(1.*_E)*dist+(2.*hmin-1.*hmax);
	double C = 1./(hmid*hmid) -1./(hmax*hmax);
	hcoupez(0,0) += C*gr(0)*gr(0)/norm ;
	hcoupez(1,1) += C*gr(1)*gr(1)/norm ;
	hcoupez(2,2) += C*gr(2)*gr(2)/norm ;
	hcoupez(1,0) = hcoupez(0,1) = C*gr(1)*gr(0)/norm;
	hcoupez(2,0) = hcoupez(0,2) = C*gr(2)*gr(0)/norm;
	hcoupez(2,1) = hcoupez(1,2) = C*gr(2)*gr(1)/norm;
      }
      H.setMat(hcoupez);
    }

    fullMatrix<double> V(3,3);
    fullVector<double> S(3);
    H.eig(V,S);
  
    double lambda1 = S(0);
    double lambda2 = S(1); 
    double lambda3 = (_dim == 3)? S(2) : 1.; 
    if (_technique != meshMetric::LEVELSET){
      lambda1 = std::min(std::max(fabs(S(0))/_epsilon,1./(hmax*hmax)),1./(hmin*hmin));
      lambda2 = std::min(std::max(fabs(S(1))/_epsilon,1./(hmax*hmax)),1./(hmin*hmin));
      lambda3 = (_dim == 3) ? std::min(std::max(fabs(S(2))/_epsilon,1./(hmax*hmax)),1./(hmin*hmin)) : 1.;
    }

    SVector3 t1 (V(0,0),V(1,0),V(2,0));
    SVector3 t2 (V(0,1),V(1,1),V(2,1));
    SVector3 t3 (V(0,2),V(1,2),V(2,2));
  
    SMetric3 metric(lambda1,lambda2,lambda3,t1,t2,t3);
    _hessian[ver] = hessian;
    _nodalMetrics[ver] = metric;
    _nodalSizes[ver] = std::min(std::min(1/sqrt(lambda1),1/sqrt(lambda2)),1/sqrt(lambda3));

    _detMetric[ver] = sqrt(metric.determinant());

    it++;	
  }

  //Adapt epsilon
  //-----------------
  // dgDofContainer dofDet(groups, 1);
  // for (int iGroup = 0; iGroup < groups->getNbElementGroups(); iGroup++){
  //     dgGroupOfElements *group = groups->getElementGroup(iGroup);
  //     fullMatrix<double> & detgroup = dofDet.getGroupProxy(group);
  //     for (int iElement = 0 ; iElement < group->getNbElements(); iElement++){
  //       MElement* e = group->getElement(iElement);
  //         for ( int i = 0; i < e->getNumVertices(); i++) {
  // 	    MVertex *ver = e->getVertex(i);
  // 	    std::map<MVertex*, double >::iterator it = _detMetric.find(ver);
  //           detgroup (i, iElement) = it->second;
  //       }
  //     }
  //   }
  // fullMatrix<double> res(1,1);
  // dgFunctionIntegrator integ(groups, dofDet.getFunction());
  // integ.compute(res);
  //fprintf("integ =%g \n", res(0,0))
    
  //Smoothing
  //-----------
  //smoothMetric (sol);
  //curvatureContributionToMetric();

  putOnNewView();

}



double meshMetric::operator() (double x, double y, double z, GEntity *ge){
  GModel *gm = _nodalMetrics.begin()->first->onWhat()->model();
  SPoint3 xyz(x,y,z),uvw;

  double initialTol = MElement::getTolerance();
  MElement::setTolerance(1.e-4); 
  MElement *e = gm->getMeshElementByCoord(xyz,_dim);    
  MElement::setTolerance(initialTol);
  if (e){
    e->xyz2uvw(xyz,uvw);
    double *val = new double [e->getNumVertices()];
    for (int i=0;i<e->getNumVertices();i++){
      val[i] = _nodalSizes[e->getVertex(i)];
    }
    double value = e->interpolate(val,uvw[0],uvw[1],uvw[2]);
    delete [] val;
    return value;    
  }
  return 1.e22;
}

void meshMetric::operator() (double x, double y, double z, SMetric3 &metr, GEntity *ge){
  metr = SMetric3(1.e-22);
  GModel *gm = _nodalMetrics.begin()->first->onWhat()->model();
  SPoint3 xyz(x,y,z),uvw;
  double initialTol = MElement::getTolerance();
  MElement::setTolerance(1.e-4); 
  MElement *e = gm->getMeshElementByCoord(xyz,_dim);    
  MElement::setTolerance(initialTol);


  if (e){
    e->xyz2uvw(xyz,uvw);
    SMetric3 m1 = _nodalMetrics[e->getVertex(0)];
    SMetric3 m2 = _nodalMetrics[e->getVertex(1)];
    SMetric3 m3 = _nodalMetrics[e->getVertex(2)];
    if (_dim == 2)
      metr =  interpolation(m1,m2,m3,uvw[0],uvw[1]);
    else {
      SMetric3 m4 = _nodalMetrics[e->getVertex(3)];
      metr =  interpolation(m1,m2,m3,m4,uvw[0],uvw[1],uvw[2]);
    }
  }
  else{
    Msg::Warning("point %g %g %g not found",x,y,z);
  }
}

// FIXME !!!!!!!!! 2 be fixed !!!
void meshMetric::setAsBackgroundMesh (GModel *gm){
  FieldManager *fields = gm->getFields();
  int id = fields->newId();
  (*fields)[id] = new meshMetric(*this);
  fields->background_field = id;
}

void meshMetric::printMetric(const char* n) const{

  FILE *f = fopen (n,"w");
  fprintf(f,"View \"\"{\n");
  std::map<MVertex*,SMetric3>::const_iterator it= _nodalMetrics.begin();
  //std::map<MVertex*,fullMatrix<double> >::const_iterator it = _hessian.begin(); 
  for (; it != _nodalMetrics.end(); ++it){
    //for (; it != _hessian.end(); ++it){
    fprintf(f,"TP(%g,%g,%g){%g,%g,%g,%g,%g,%g,%g,%g,%g};\n",
	    it->first->x(),it->first->y(),it->first->z(),
	    it->second(0,0),it->second(0,1),it->second(0,2),
	    it->second(1,0),it->second(1,1),it->second(1,2),
	    it->second(2,0),it->second(2,1),it->second(2,2)
	    );
  } 
  fprintf(f,"};\n");
  fclose (f);    
}

double meshMetric::getLaplacian (MVertex *v){

  std::map<MVertex*, fullMatrix<double> >::iterator it = _hessian.find(v);
  fullMatrix<double> h = it->second;
  double laplace = h(0,0)+h(1,1)+h(2,2);
  return laplace; 
}


/*void dgMeshMetric::curvatureContributionToMetric (){
  std::map<MVertex*,SMetric3>::iterator it = _nodalMetrics.begin();
  std::map<MVertex*,double>::iterator its = _nodalSizes.begin();
  for (; it != _nodalMetrics.end(); ++it,++its){
  MVertex *v = it->first;
  if (v->onWhat()->dim() == 0){
  double l;
  SMetric3 curvMetric = max_edge_curvature_metric((GVertex*)v->onWhat(),l);
  it->second = intersection(it->second,curvMetric);
  its->second = std::min(l,its->second);
  } 
  else if (v->onWhat()->dim() == 1){
  double u,l;
  v->getParameter(0,u);
  SMetric3 curvMetric = max_edge_curvature_metric((GEdge*)v->onWhat(),u,l);
  it->second = intersection(it->second,curvMetric);
  its->second = std::min(l,its->second);
  } 
  }
  }*/

/*
  void dgMeshMetric::smoothMetric (dgDofContainer *sol){
  return;
  dgGroupCollection *groups = sol->getGroups();
  std::set<MEdge,Less_Edge> edges;
  for (int i=0;i<groups->getNbElementGroups();i++){
  dgGroupOfElements *group = groups->getElementGroup(i);
  for (int j=0;j<group->getNbElements();j++){
  MElement *e = group->getElement(j);
  for (int k=0;k<e->getNumEdges();k++){
  edges.insert(e->getEdge(k));
  }
  }
  }

  std::set<MEdge,Less_Edge>::iterator it = edges.begin();
  double logRatio = log (CTX::instance()->mesh.smoothRatio);
  for ( ; it !=edges.end(); ++it){
  MEdge e = *it;
  std::map<MVertex*,SMetric3>::iterator it1 = _nodalMetrics.find(e.getVertex(0));
  std::map<MVertex*,SMetric3>::iterator it2 = _nodalMetrics.find(e.getVertex(1));
  SVector3 aij (e.getVertex(1)->x()-e.getVertex(0)->x(),
  e.getVertex(1)->y()-e.getVertex(0)->y(),
  e.getVertex(1)->z()-e.getVertex(0)->z());
  SMetric3 m1 = it1->second;
  double al1 = sqrt(dot(aij,m1,aij));
  SMetric3 m2 = it2->second;
  double al2 = sqrt(dot(aij,m2,aij));
  //    it1->second.print("m1");
  //    it2->second.print("m2");
  m1 *= 1./((1+logRatio*al1)*(1+logRatio*al1));
  m2 *= 1./((1+logRatio*al2)*(1+logRatio*al2));
  it1->second = intersection (it1->second,m2);
  it2->second = intersection (it2->second,m1);
  //    it1->second.print("m1 after");
  //    it2->second.print("m2 after");
  //    getchar();
  }
  }
*/
