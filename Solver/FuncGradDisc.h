//
// Description : Heaviside function based on level set discontinuity description
//
//
// Author:  <Boris Sedji>,  01/2010
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef _FUNCGRADDISC_H_
#define _FUNCGRADDISC_H_

#include "simpleFunction.h"
#include "DILevelset.h"
#include "MVertex.h"
#include "GModel.h"
//#include "gLevelSetMesh.cpp"


class FuncGradDisc :  public  simpleFunctionOnElement<double> {
 private :

 gLevelset *_ls;
 GModel * _pModel;


 public :
  FuncGradDisc(gLevelset *ls,GModel *pModel){
		_ls = ls;
		_pModel = pModel;

  }

  double operator () (double x,double y,double z) const {


        // --- F2 --- //
        MElement *e=getElement();
        SPoint3 p(x,y,z);

        if (e->getParent()) e = e->getParent();
        int numV = e->getNumVertices();
        double xyz[3] = {x,y,z};
        double uvw[3];
        e->xyz2uvw(xyz,uvw);
        double val[30];
        e->getShapeFunctions(uvw[0], uvw[1], uvw[2], val);
        double f = 0;
        for (int i = 0;i<numV;i++)
        {
           //std::cout<<"val[i] :" << val[i] << "\n";
           //std::cout<<"ls(i) :" << (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()) << "\n";
           f = f + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*val[i];
        }
           f = f - std::abs((*_ls)(x,y,z));

        //std::cout<<"val f :" << f << "\n";
        return f;


          // --- F1 --- //


//        SPoint3 p(x,y,z);
//        if (e->getParent()) e = e->getParent();
//        int numV = e->getNumVertices();
//        double xyz[3] = {x,y,z};
//        double uvw[3];
//        e->xyz2uvw(xyz,uvw);
//        double val[30];
//        e->getShapeFunctions(uvw[0], uvw[1], uvw[2], val);
//        double f = 0;
//        for (int i = 0;i<numV;i++)
//        {
//           f = f + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*val[i];
//        }
//        f = std::abs(f);
//        return f;

  }



  void gradient (double x, double y, double z,
			 double & dfdx, double & dfdy, double & dfdz) const
  {


        // ---- F2 ---- //
        MElement *e=getElement();
        SPoint3 p(x,y,z);
        if (e->getParent()) e = e->getParent();
        int numV = e->getNumVertices();
        double xyz[3] = {x,y,z};
        double uvw[3];
        e->xyz2uvw(xyz,uvw);
        double gradsuvw[256][3];
        e->getGradShapeFunctions(uvw[0],uvw[1],uvw[2],gradsuvw);

        double jac[3][3];
        double invjac[3][3];
        double dNdx;
        double dNdy;
        double dNdz;
        const double detJ = e->getJacobian(uvw[0], uvw[1], uvw[2], jac);
        inv3x3(jac, invjac);

        dfdx = 0;
        dfdy = 0;
        dfdz = 0;

        if ((*_ls)(x,y,z)>0)
        {
          for (int i = 0;i<numV;i++)
          {
            dNdx = invjac[0][0] * gradsuvw[i][0] + invjac[0][1] * gradsuvw[i][1] + invjac[0][2] * gradsuvw[i][2];
            dNdy = invjac[1][0] * gradsuvw[i][0] + invjac[1][1] * gradsuvw[i][1] + invjac[1][2] * gradsuvw[i][2];
            dNdz = invjac[2][0] * gradsuvw[i][0] + invjac[2][1] * gradsuvw[i][1] + invjac[2][2] * gradsuvw[i][2];

            dfdx = dfdx + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*dNdx ;
            dfdx = dfdx - (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdx;
            dfdy = dfdy + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*dNdy ;
            dfdy = dfdy - (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdy;
            dfdz = dfdz + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*dNdz ;
            dfdz = dfdz - (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdz;
          }
        }else
        {
          for (int i = 0;i<numV;i++)
          {
            dNdx = invjac[0][0] * gradsuvw[i][0] + invjac[0][1] * gradsuvw[i][1] + invjac[0][2] * gradsuvw[i][2];
            dNdy = invjac[1][0] * gradsuvw[i][0] + invjac[1][1] * gradsuvw[i][1] + invjac[1][2] * gradsuvw[i][2];
            dNdz = invjac[2][0] * gradsuvw[i][0] + invjac[2][1] * gradsuvw[i][1] + invjac[2][2] * gradsuvw[i][2];

            dfdx = dfdx + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*dNdx ;
            dfdx = dfdx + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdx;
            dfdy = dfdy + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*dNdy ;
            dfdy = dfdy + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdy;
            dfdz = dfdz + std::abs((*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z()))*dNdz ;
            dfdz = dfdz + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdz;
          }
        }
   }


        // ---- F1 ------ //

//
//        SPoint3 p(x,y,z);
//        if (e->getParent()) e = e->getParent();
//        int numV = e->getNumVertices();
//        double xyz[3] = {x,y,z};
//        double uvw[3];
//        e->xyz2uvw(xyz,uvw);
//        double gradsuvw[256][3];
//        e->getGradShapeFunctions(uvw[0],uvw[1],uvw[2],gradsuvw);
//
//        double jac[3][3];
//        double invjac[3][3];
//        double dNdx;
//        double dNdy;
//        double dNdz;
//        const double detJ = e->getJacobian(uvw[0], uvw[1], uvw[2], jac);
//        inv3x3(jac, invjac);
//
//        dfdx = 0;
//        dfdy = 0;
//        dfdz = 0;
//
//        if ((*_ls)(x,y,z)>0)
//        {
//          for (int i = 0;i<numV;i++)
//          {
//            dNdx = invjac[0][0] * gradsuvw[i][0] + invjac[0][1] * gradsuvw[i][1] + invjac[0][2] * gradsuvw[i][2];
//            dNdy = invjac[1][0] * gradsuvw[i][0] + invjac[1][1] * gradsuvw[i][1] + invjac[1][2] * gradsuvw[i][2];
//            dNdz = invjac[2][0] * gradsuvw[i][0] + invjac[2][1] * gradsuvw[i][1] + invjac[2][2] * gradsuvw[i][2];
//
//            dfdx = dfdx + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdx;
//            dfdy = dfdy + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdy;
//            dfdz = dfdz + (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdz;
//          }
//        }else
//        {
//          for (int i = 0;i<numV;i++)
//          {
//            dNdx = invjac[0][0] * gradsuvw[i][0] + invjac[0][1] * gradsuvw[i][1] + invjac[0][2] * gradsuvw[i][2];
//            dNdy = invjac[1][0] * gradsuvw[i][0] + invjac[1][1] * gradsuvw[i][1] + invjac[1][2] * gradsuvw[i][2];
//            dNdz = invjac[2][0] * gradsuvw[i][0] + invjac[2][1] * gradsuvw[i][1] + invjac[2][2] * gradsuvw[i][2];
//
//            dfdx = dfdx - (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdx;
//            dfdy = dfdy - (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdy;
//            dfdz = dfdz - (*_ls)(e->getVertex(i)->x(),e->getVertex(i)->y(),e->getVertex(i)->z())*dNdz;
//          }
//        }
//   }


};

#endif