// Gmsh - Copyright (C) 1997-2008 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to <gmsh@geuz.org>.

#ifndef _PVIEW_DATA_GMODEL_H_
#define _PVIEW_DATA_GMODEL_H_

#include "PViewData.h"
#include "GModel.h"
#include "SBoundingBox3d.h"

template<class real>
class stepData{
 private:
  // a pointer to the underlying model
  GModel *_model;
  // the unrolled list of all geometrical entities in the model
  std::vector<GEntity*> _entities;
  // the bounding box of the view
  SBoundingBox3d _bbox;
  // the file the data was read from (if empty, refer to PViewData)
  std::string _fileName;
  // the index in the file (if negative, refer to PViewData)
  int _fileIndex;
  // the value of the time step and value min/max
  double _time, _min, _max;
  // the number of components in the data (one stepData contains only
  // a single field type)
  int _numComp;
  // the values, indexed by MVertex or MElement id numbers (If the
  // numbering is sparse, or if we only have data for high-id
  // entities, the vector has zero entries and is thus not
  // optimal. This is the price to pay if we want 1) rapid access to
  // the data and 2) not to store any additional info in MVertex or
  // MElement)
  std::vector<real*> *_data;
  // a vector, indexed by MSH element type, of Gauss point locations
  // in parametric space
  std::vector<std::vector<double> > _gaussPoints;
 public:
  stepData(GModel *model, int numComp, std::string fileName="", int fileIndex=-1, 
	   double time=0., double min=VAL_INF, double max=-VAL_INF)
    : _model(model), _fileName(fileName), _fileIndex(fileIndex), _time(time), 
      _min(min), _max(max), _numComp(numComp), _data(0)
  {
    _entities = _model->getEntities();
    _bbox = _model->bounds();
  }
  ~stepData(){ destroyData(); }
  GModel *getModel(){ return _model; }
  SBoundingBox3d getBoundingBox(){ return _bbox; }
  int getNumEntities(){ return _entities.size(); }
  GEntity *getEntity(int ent){ return _entities[ent]; }
  int getNumComponents(){ return _numComp; }
  std::string getFileName(){ return _fileName; }
  void setFileName(std::string name){ _fileName = name; }
  int getFileIndex(){ return _fileIndex; }
  void setFileIndex(int index){ _fileIndex = index; }
  double getTime(){ return _time; }
  void setTime(double time){ _time = time; }
  double getMin(){ return _min; }
  void setMin(double min ){ _min = min; }
  double getMax(){ return _max; }
  void setMax(double max){ _max = max; }
  int getNumData()
  {
    if(!_data) return 0;
    return _data->size();
  }
  void resizeData(int n)
  {  
    if(!_data) _data = new std::vector<real*>(n, (real*)0);
    if(n > (int)_data->size()) _data->resize(n, (real*)0);
  }
  real *getData(int index, bool allocIfNeeded=false, int mult=1)
  {
    if(allocIfNeeded){
      if(index >= getNumData()) resizeData(index + 100); // optimize this
      if(!(*_data)[index]){
	(*_data)[index] = new real[_numComp * mult];
	for(int i = 0; i < _numComp * mult; i++) (*_data)[index][i] = 0.;
      }
    }
    else{
      if(index >= getNumData()) return 0;
    }
    return (*_data)[index];
  }
  void destroyData()
  {
    if(_data){
      for(unsigned int i = 0; i < _data->size(); i++)
        if((*_data)[i]) delete [] (*_data)[i];
      delete _data;
      _data = 0;
    }
  }
  std::vector<double> &getGaussPoints(int msh)
  {
    if((int)_gaussPoints.size() <= msh) _gaussPoints.resize(msh + 1);
    return _gaussPoints[msh];
  }
};

// The data container using elements from one or more GModel(s).
class PViewDataGModel : public PViewData {
 public:
  enum DataType {
    NodeData = 1,
    ElementData = 2,
    ElementNodeData = 3,
    GaussPointData = 4
  };
 private:
  // the data, indexed by time step
  std::vector<stepData<double>*> _steps;
  // the global min/max of the view
  double _min, _max;
  // a set of all "partitions" encountered in the input data
  std::set<int> _partitions;
  // the type of the dataset
  DataType _type;
  // cache last element to speed up loops
  MElement *_getElement(int step, int ent, int ele);
 public:
  PViewDataGModel(DataType type=NodeData);
  ~PViewDataGModel();
  bool finalize();
  int getNumTimeSteps();
  double getTime(int step);
  double getMin(int step=-1);
  double getMax(int step=-1);
  SBoundingBox3d getBoundingBox(int step=-1);
  int getNumScalars(int step=-1);
  int getNumVectors(int step=-1);
  int getNumTensors(int step=-1);
  int getNumPoints(int step=-1){ return 0; }
  int getNumLines(int step=-1);
  int getNumTriangles(int step=-1);
  int getNumQuadrangles(int step=-1);
  int getNumTetrahedra(int step=-1);
  int getNumHexahedra(int step=-1);
  int getNumPrisms(int step=-1);
  int getNumPyramids(int step=-1);
  int getNumEntities(int step=-1);
  int getNumElements(int step=-1, int ent=-1);
  int getDimension(int step, int ent, int ele);
  int getNumNodes(int step, int ent, int ele);
  int getNode(int step, int ent, int ele, int nod, double &x, double &y, double &z);
  void setNode(int step, int ent, int ele, int nod, double x, double y, double z);
  void tagNode(int step, int ent, int ele, int nod, int tag);
  int getNumComponents(int step, int ent, int ele);
  int getNumValues(int step, int ent, int ele);
  void getValue(int step, int ent, int ele, int idx, double &val);
  void getValue(int step, int ent, int ele, int node, int comp, double &val);
  void setValue(int step, int ent, int ele, int node, int comp, double val);
  int getNumEdges(int step, int ent, int ele);
  void revertElement(int step, int ent, int ele);
  void smooth();
  bool skipEntity(int step, int ent);
  bool skipElement(int step, int ent, int ele, bool checkVisibility=false);
  bool hasTimeStep(int step);
  bool hasPartition(int part);
  bool hasMultipleMeshes();
  bool hasModel(GModel *model, int step=-1);
  bool useGaussPoints(){ return _type == GaussPointData; }

  // get the data type
  DataType getType(){ return _type; }
  // direct access to GModel entities
  GEntity *getEntity(int step, int ent);
  // direct access to value by index
  bool getValueByIndex(int step, int dataIndex, int node, int comp, double &val);
  // get underlying model
  GModel* getModel(int step){ return _steps[step]->getModel(); }

  // Add some data "on the fly" (data is stored by vertex: if a field
  // has e.g. 3 components, nodalData contains 3 * N entries with N
  // being the number of mesh vertices in the model ; nodalData [ iVer
  // * N + jComp] is the jComp-th component at vertex iVer)
  bool addNodalData(int step, double time, int partition, 
		    int numComp, const std::vector<double> &nodalData);

  // I/O routines
  bool readMSH(std::string fileName, int fileIndex, FILE *fp, bool binary, 
               bool swap, int step, double time, int partition, 
               int numComp, int numNodes);
  bool writeMSH(std::string fileName, bool binary=false);
  bool readMED(std::string fileName, int fileIndex);
  bool writeMED(std::string fileName);
};

#endif