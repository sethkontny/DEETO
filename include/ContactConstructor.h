// TODO : 
//  - Scrivere la descrizione dell'algoritmo di update per doxygen
//  - Le costanti usate, farle diventare tali
#ifndef CONTACT_CONSTRUCTOR_H
#define CONTACT_CONSTRUCTOR_H

#include "Definitions.h"
#include "Electrode.h"
#include "ClinicalFrame.h"
#include "itkImageToListSampleAdaptor.h"
#include "itkHistogram.h"
#include "itkSampleToHistogramFilter.h"
#include "itkMinimumMaximumImageCalculator.h"

class ContactConstructor {

 public:
  const static double MAX_ANGLE  = 0.988;

  //double maxAngle= 0.984807753; // dieci gradi
  //double maxAngle = 0.996194698; // cinque gradi

  ContactConstructor(  ImageType::Pointer ctImage, ClinicalFrame* headFrame, TCLAP::CmdLine* c ) {
    ctImage_   = ctImage;
    headFrame_ = headFrame;
    
    //loadStatistics_();
  }

  ContactConstructor(  ImageType::Pointer ctImage, ClinicalFrame* headFrame) {
    ctImage_   = ctImage;
    headFrame_ = headFrame;
    calcolaThreshold_();
  }

  ~ContactConstructor(){}
  
  void update( void );
  
 private:
  ImageType::Pointer ctImage_;
  ClinicalFrame*     headFrame_;
  
  double             angle_; 
  double             threshold_; // Threshold for the intensity point

  RegionType retriveRegionAroundContact_(VoxelPointType index, int regionDim);
  void printContact_(string name,int  number,PhysicalPointType point);
  double getValue_(PhysicalPointType point);
  double getValue_(PhysicalPointType point, double regionSize); // questo perche' un centro di massa potrebbe aver valore nullo
  PhysicalPointType getPointWithHigherMoment_(PhysicalPointType center, int minRegionSize, int maxRegionSize);
  PhysicalPointType getNextContact_(PhysicalPointType c, PhysicalPointType c1, PhysicalPointType c2, double distance);
  PhysicalPointType lookForTargetPoint_(PhysicalPointType entryPoint, PhysicalPointType targetPoint, int regionSize, int maxRegionSize);
  PhysicalPointType lookForEntryPoint_(PhysicalPointType entryPoint,int regionSize, int maxRegionSize);
  void printRegion_(RegionType region);
  double computeCos(PhysicalPointType a1,PhysicalPointType a2,PhysicalPointType b1,PhysicalPointType b2);
  double loadStatistics_( void );
  double distanceToSurface_(PhysicalPointType p1,PhysicalPointType p2,PhysicalPointType p);
  void calcolaThreshold_( void );
};

// Calcola il punto con il maggior momento in un cubo di larghezza regionSize e centro center. 
// Se il momento e' nullo incrementa la dimensione fino ad un massimo di maxRegioneSize
// Se all'interno della regione non si trova un punto con un momento
// != da 0 allora ritorna il punto stesso.
// Notice: che dipende fortemente da come e' orientata la retta su cui cerchiamo i punti, da fare qualcosa?
PhysicalPointType ContactConstructor::getPointWithHigherMoment_(PhysicalPointType center, int minRegionSize, int maxRegionSize){
  VoxelPointType    vcenter; // Center in voxel coordinates
  FilterType::Pointer filter = FilterType::New();
  CalculatorType::Pointer calculator = CalculatorType::New();
  filter->SetInput(ctImage_);
  double regionSize = minRegionSize;
  do {
    ctImage_->TransformPhysicalPointToIndex(center,vcenter);
    RegionType region = retriveRegionAroundContact_(vcenter,regionSize);
    if(ctImage_->GetLargestPossibleRegion().IsInside( region )){
      filter->SetRegionOfInterest(region );
      calculator->SetImage(filter->GetOutput());
      try {
	filter->Update();
	calculator->Compute();
	center[0] = calculator->GetCenterOfGravity()[0]; 
	center[1] = calculator->GetCenterOfGravity()[1]; 
	center[2] = calculator->GetCenterOfGravity()[2]; 
	//cout << "M " << calculator->GetTotalMass() << endl;
	return center; 
      } catch (itk::ExceptionObject &ex) {
	//cerr<<"Error : " << "Momento Nullo" << __LINE__<<ex.what()<<endl;
	regionSize += 1;
      }
    } else {
      maxRegionSize--;
    }
  } while (regionSize < maxRegionSize);
  return center;
}

void ContactConstructor::update( void ){
  ClinicalFrame::ElectrodeIterator electrodeItr = headFrame_->begin();
  
  while(electrodeItr != headFrame_->end()) {
    PhysicalPointType entryPoint = electrodeItr->getEntry(); // set the first contact to the entry point
    PhysicalPointType targetPoint = electrodeItr->getTarget(); // set the first contact to the entry point
    //printContact_(electrodeItr->getName(),0,entryPoint);
    // printContact_(electrodeItr->getName(),0,targetPoint);

    int regionSize = 3;
    int maxRegionSize = 7;
    int k = 1;
    entryPoint = lookForEntryPoint_(entryPoint,regionSize,maxRegionSize);
    targetPoint = lookForTargetPoint_(entryPoint,targetPoint,regionSize,(maxRegionSize - 2)); // -2 perche' in teoria si e' sulla linea giusta ...
    PhysicalPointType c;                   // candidate
    PhysicalPointType cprime;              // best candidate
    PhysicalPointType r1 = targetPoint;    // primo punto per calcolare la retta
    PhysicalPointType r2 = entryPoint;     // secondo punto per calcolare la retta
    PhysicalPointType start = targetPoint; // punto a partire dal quale si calcola
                                           // il nuovo punto sulla retta r1-r2
    double angle = 0.0;
    double distance = 3.5;
    double delta = 0.3;
    int rs = regionSize;

    /* printContact_("REP",0,entryPoint); */
    /* printContact_("RTP",0,targetPoint); */
    // pezza perche' il target point va fuori 
    if (getValue_(targetPoint,rs) > threshold_*3) {
      printContact_(electrodeItr->getName(),k,targetPoint);
      electrodeItr->addContact(targetPoint);
      k++;
    }
    // Inizio cerca dei punti
    do {
      // punto candidato
      c = getNextContact_(start,r1,r2,distance);
      //printContact_("C",k,c);
      // cout << getValue_(c,rs) << ";" << rs << endl;
      // centro di massa
      cprime = getPointWithHigherMoment_(c,rs,rs);
      //printContact_("P'",k,cprime);
      //cout << getValue_(cprime,rs) << ";" << rs << endl;
      // calcolo angolo di deviazione tra il cprime e i punti precedenti
      if ((r2.EuclideanDistanceTo(cprime) < 0.001) || (r2.EuclideanDistanceTo(entryPoint) < 0.001)) 
	angle = computeCos(r1,r2,r1,cprime);
      else angle = computeCos(r1,r2,r2,cprime);
      
      //cout << angle << " : " << r2.EuclideanDistanceTo(cprime) << " : " << getValue_(cprime,rs) << " : " << rs << endl;

      if((angle <= MAX_ANGLE) && (rs > 1)) {
		//distance = 3.2;
		rs -= 1;
		//cout << "Region Size " << rs << endl;
		//  printContact_("S",k,n2);
      } else if ((r2.EuclideanDistanceTo(entryPoint) > 0.1) &&                            // se non e' il primo punto.
		 ((r2.EuclideanDistanceTo(cprime) > (distance + delta)) ||
		  (r2.EuclideanDistanceTo(cprime)  < (distance - delta))) && (rs > 1)) {
	rs -= 1;
      } else { 
	/*Inutile se rs = 1 allora cprime e c differiscono di pochissimo*/
	/* if (rs == 1) { */
	/*   if (//(angle <= MAX_ANGLE) ||  */
	/*       ((r2.EuclideanDistanceTo(entryPoint) > 0.1) &&                            // se non e' il primo punto. */
	/*       ((r2.EuclideanDistanceTo(cprime) > (distance + delta)) || */
	/*        (r2.EuclideanDistanceTo(cprime)  < (distance - delta))))) */
	/*     if() */
	/*     cprime = c; */
	/* } */

	// getValue2
	// calcola il momento come sommatoria dei punti intorno a
	// cprime, regione grande rs. almeno un terzo dei punti deve
	// avere un valore sopra la soglia	
	
	if (getValue_(cprime,rs)  > (threshold_)) { 
	  rs = regionSize;
	  if (r1.EuclideanDistanceTo(targetPoint) > 0.1) r1 = r2;
	  //cout << "D: " << r2.EuclideanDistanceTo(cprime) << endl;
	  r2 = cprime;
	  start = cprime;
	  electrodeItr->addContact(cprime);
	  printContact_(electrodeItr->getName(),k,cprime);
	  k++;
	}
      }
    }while((getValue_(cprime,rs)  > (threshold_)) && (rs >= 1) && (k<21));
    electrodeItr++;
  }
}

RegionType ContactConstructor::retriveRegionAroundContact_(VoxelPointType index, int regionDim) {
  SizeType size;
  RegionType region;
  SpacingType spacing = ctImage_->GetSpacing();
  //PhysicalPointType tmp;
  //ctImage_->TransformIndexToPhysicalPoint(index,tmp);
  //printContact_("C",0,tmp);
  for(unsigned int i=0;i<3;i++) {
    size[i]    = 2*regionDim+1;
    index[i]   -= regionDim;    
  }
  region.SetSize(size);
  region.SetIndex(index);
  //ctImage_->TransformIndexToPhysicalPoint(index,tmp);
  //  printContact_("C",1,tmp);  
  //printRegion_(region);

  return region;
}

void ContactConstructor::printContact_(string name,int number,PhysicalPointType point) {
  cout << name << number<<","<< point[0] << "," << point[1] << "," << point[2] <<",0,0" <<endl;  
}

double ContactConstructor::getValue_(PhysicalPointType point) {
  VoxelPointType    voxelPoint;
  ctImage_->TransformPhysicalPointToIndex(point,voxelPoint);

  if(ctImage_->GetLargestPossibleRegion().IsInside(voxelPoint) )
	  return ctImage_->GetPixel(voxelPoint);
    else 
	  return 0.0;
}

double ContactConstructor::getValue_(PhysicalPointType point, double regionSize) {
  VoxelPointType    vcenter;
  ctImage_->TransformPhysicalPointToIndex(point,vcenter);
  RegionType region = retriveRegionAroundContact_(vcenter,regionSize);

  if(ctImage_->GetLargestPossibleRegion().IsInside(vcenter) &&
		  ctImage_->GetLargestPossibleRegion().IsInside(region)){

	  itk::ImageRegionIterator<ImageType> imageIterator(ctImage_,region);
	  VoxelPointType p;
	  PhysicalPointType tmp;  
	  int num = 0;
	  double sum = 0.0;
	  while(!imageIterator.IsAtEnd()){
		p = imageIterator.GetIndex();
		ctImage_->TransformIndexToPhysicalPoint(p,tmp);
		sum += getValue_(tmp);
		num++;
		++imageIterator;
	  }
	  if (num > 0) return sum;
	  return 0;
  }else return 0.0;
}


// Return a new contact:
// contact has an euclidean "distance" with "c", on the line "c1"-"c2"
PhysicalPointType ContactConstructor::getNextContact_(PhysicalPointType c, PhysicalPointType c1, PhysicalPointType c2, double distance) {
  double t = (c.EuclideanDistanceTo(c1) + distance) / c1.EuclideanDistanceTo(c2);
  PhysicalPointType nextC;
  nextC[0] = c1[0] + (c2[0] - c1[0]) * t; // x
  nextC[1] = c1[1] + (c2[1] - c1[1]) * t; // y
  nextC[2] = c1[2] + (c2[2] - c1[2]) * t; // z
  return nextC;
}

// Look for an Entry point
PhysicalPointType ContactConstructor::lookForEntryPoint_(PhysicalPointType entryPoint,int regionSize, int maxRegionSize){
  PhysicalPointType n1 = entryPoint;
  PhysicalPointType n2;
  uint k = 0;
  do {
    n2 = getPointWithHigherMoment_(n1,regionSize,maxRegionSize);
    if(n2.EuclideanDistanceTo(n1) < 0.1){ // n1 == n2
      regionSize += 1;
    } else {
      n1 = n2;
      //printContact_("CC",k,n1);
      k++;
    }
    
  } while (regionSize <= maxRegionSize);
  return n2;
}


// Look for the target point (e' la punta dell'elettrodo)
PhysicalPointType ContactConstructor::lookForTargetPoint_(PhysicalPointType entryPoint, PhysicalPointType targetPoint, int regionSize, int maxRegionSize){

  PhysicalPointType p;  
  PhysicalPointType c;     // candidate
  PhysicalPointType p1;    // primo punto per calcolare la retta
  PhysicalPointType p2;    // secondo punto per calcolare la retta
  PhysicalPointType start; // punto a partire dal quale si calcola il nuovo punto giacente sulla retta r1-r2
  double angle = 0.0;
  double distance = 3.2;
  int rs = regionSize;
  int k = 0;
  p1 = entryPoint;
  p2 = targetPoint;
  start = entryPoint;
  
  double size = 0.0;
  double d;  // E' la distanza tra il punto corrente e il piano che taglia a meta' il cervello nella direzione ortogonale alla retta su cui giace il punto corrente
  double d1; // E' la distanza tra l'ultimo punto calcolato con un valore significativo e il punto corrente.
  // Scelgo come end point il piu' lontano tra target e/o il piano che
  // passa per l'origine.
  double riferimento1 = distanceToSurface_(entryPoint,targetPoint,entryPoint);
  double riferimento2 = entryPoint.EuclideanDistanceTo(targetPoint);
  double riferimento = (riferimento1 > riferimento2 ? riferimento2 : riferimento1);
  /* PhysicalPointType pp = getNextContact_(entryPoint,entryPoint,targetPoint,riferimento1); */
  /* printContact_("EE",0,pp); */
  double maxDelta = -1.5;
  do {
    // punto candidato
    c = getNextContact_(start,p1,p2,distance);
    d = riferimento - entryPoint.EuclideanDistanceTo(c);
    while((getValue_(c,3) < threshold_) && ((d >= maxDelta) || (d1 < 6.0))){
      distance += 1.0;
      c = getNextContact_(p2,p1,p2,distance);
      d = riferimento - entryPoint.EuclideanDistanceTo(c); // distanza dal piano mezzano
      d1 = c.EuclideanDistanceTo(p2);
      //cout << "DIST: " << d1 << ";" << d << ";" << distance<< endl;
    }
    //printContact_("C",k,c);
    // centro di massa
    if ((d >= maxDelta) || (d1 < 6.0)){ 
      p = getPointWithHigherMoment_(c,rs,rs);
      // calcolo l'angolo tra il centro e i precedenti punti per vedere se ha deviato.
      //printContact_("P",k,p);
      if ((p2.EuclideanDistanceTo(p) < 0.001) || (p2.EuclideanDistanceTo(targetPoint) < 0.001)) {
	angle = computeCos(p1,p2,p1,p);
	d1 = 1.0;
      } else {
	angle = computeCos(p1,p2,p2,p);
	d1 = p.EuclideanDistanceTo(p2);
      }
      d = riferimento - entryPoint.EuclideanDistanceTo(p);
      //cout << "DIST1: " << d1 << ";" << d << endl;
      if((angle <= MAX_ANGLE) && (rs > 1)){
	rs -= 1;
      } else {
	distance = 3.2;
	rs = regionSize;
	if (p2.EuclideanDistanceTo(targetPoint) > 0.1) p1 = p2;
	p2 = p;
	start = p;
	//printContact_("T",k,p);
	k++;
      }
    } else {
      if (getValue_(c,regionSize) > getValue_(p,regionSize)) p = c;
    }
  } while ((d >= maxDelta) || (d1 < 6.0));
  distance = 1.0;
  //printContact_("x",k,p);
  c = p;
  while (getValue_(c,regionSize) < regionSize*threshold_) {
    c = getNextContact_(p2,p2,p1,distance);
    distance += 0.5;
    //printContact_("x",k,c);
  }
  c = getPointWithHigherMoment_(c,regionSize,regionSize);
  //printContact_("RTP",k,c);
  return c;
}

void ContactConstructor::printRegion_(RegionType region){
  itk::ImageRegionIterator<ImageType> imageIterator(ctImage_,region);
  VoxelPointType p;
  PhysicalPointType tmp;  

  while(!imageIterator.IsAtEnd()){
    p = imageIterator.GetIndex();
    ctImage_->TransformIndexToPhysicalPoint(p,tmp);
    printContact_("",0,tmp);
    //cout << "0,"<<p << endl;
    ++imageIterator;
  }
}

/* Date due rette r ed s nello spazio aventi rispettivamente i
   parametri direttori v_1(l1, m1, n1) e v2(l2, m2, n2) */
/* l’angolo tra le due rette si ottiene da */

/*cos rs = l1*l2+m1*m2+n1*n2/modulo v1 * modulo v2*/
double ContactConstructor::computeCos(PhysicalPointType a1,PhysicalPointType a2,PhysicalPointType b1,PhysicalPointType b2){
  double l1 = a1[0] - a2[0];
  double m1 = a1[1] - a2[1];
  double n1 = a1[2] - a2[2];
  double l2 = b1[0] - b2[0];
  double m2 = b1[1] - b2[1];
  double n2 = b1[2] - b2[2];
  double arcos = (l1*l2 + m1*m2+n1*n2)/(sqrt(l1*l1+m1*m1+n1*n1)*sqrt(l2*l2+m2*m2+n2*n2));
  if (arcos > 0) return arcos;
  else return -1*arcos;
}


double ContactConstructor::loadStatistics_( void ) {
  // Non va bene, bisognerebbe considerare solo i valori > di una soglia minima 
  // visto che ci sono troppi punti = a 0;
  // Idea : fare una regione grande quanto un immagine (size = 255/255/255?)
  // Fare un filtro e iterare sulla regione e calcolare Mean/std/Min/Max a mano)
  ImageType::RegionType region = ctImage_->GetLargestPossibleRegion();
  itk::ImageRegionIterator<ImageType> imageIterator(ctImage_,region);
  double sum = 0.0;
  double value = 0.0;
  double std  = 0.0; 
  double mean = 0.0; 
  double min  = 115500.0; // num a caso molto grosso
  double max  = 0.0;
  int num = 0;
    
  while(!imageIterator.IsAtEnd()){
    
    value = ctImage_->GetPixel(imageIterator.GetIndex());
    if (value >= threshold_) {
      sum += value;
      max = (value >= max ? value : max);
      min = (value < min ? value : min);
      num++;
    }
    ++imageIterator;
  }
  mean = (num > 0 ? sum/num : 0.0);

  
  // standard deviation
  sum = 0.0;
  itk::ImageRegionIterator<ImageType> imageIterator2(ctImage_,region);
  while(!imageIterator2.IsAtEnd()){  
    value = ctImage_->GetPixel(imageIterator2.GetIndex());
    if (value >= threshold_) {
      sum += (mean - value) * (mean - value);
    }
    ++imageIterator2;
  }
  std = sqrt(sum)/num;
  cout << "min    :" << min << endl; 
  cout << "max :" << max << endl; 
  cout << "MEAN   : " << mean << endl;
  cout << "STD   : " << std << endl;
}

/* 
   Compute the distance between the point p from the surface passing
   for O(0,0,0) (RAS) and having vector director ortogonal to the
   stright line passing for (P2-P1)
*/
double ContactConstructor::distanceToSurface_(PhysicalPointType p1,PhysicalPointType p2,PhysicalPointType p){
  double d = abs((p2[0]-p1[0])*p[0] + (p2[1]-p1[1])*p[1] + (p2[2]-p1[2])*p[2]) / sqrt(pow((p2[0]-p1[0]),2.0) + pow((p2[1]-p1[1]),2.0) + pow((p2[2]-p1[2]),2.0));
  return d;
}

void ContactConstructor::calcolaThreshold_( void ) {

  // Questo calcola l'istogramma, ma non mi e' chiarissimo.
  typedef itk::Statistics::ImageToListSampleAdaptor< ImageType > AdaptorType;
  AdaptorType::Pointer adaptor = AdaptorType::New();
  adaptor->SetImage(ctImage_ );
  typedef short HistogramMeasurementType;
  typedef itk::Statistics::Histogram< HistogramMeasurementType > HistogramType;
  typedef itk::Statistics::SampleToHistogramFilter< AdaptorType, HistogramType > FilterType;
  FilterType::Pointer filter = FilterType::New();


  typedef itk::MinimumMaximumImageCalculator <ImageType> ImageCalculatorFilterType;
  
  ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New ();
  imageCalculatorFilter->SetImage(ctImage_);
  imageCalculatorFilter->Compute();
  
  HistogramType::SizeType size( 1 );
  size.Fill(255);
  filter->SetInput( adaptor );
  filter->SetHistogramSize( size );
  filter->SetMarginalScale( 10 );
  HistogramType::MeasurementVectorType min( 1 );
  HistogramType::MeasurementVectorType max( 1 );
  min.Fill( 0 );
  max.Fill( 255 );
  filter->SetHistogramBinMinimum( min );
  filter->SetHistogramBinMaximum( max );
  filter->Update();
  HistogramType::ConstPointer histogram = filter->GetOutput();
  const unsigned int histogramSize = histogram->Size();
  
  double maxValue = imageCalculatorFilter->GetMaximum();
  double totFreq = histogram->GetTotalFrequency();
  double freqCumulativa = 0.0;
  double sommaCumulativa = 0.0;
  unsigned int bin=0;
  while (freqCumulativa <= 0.99972) {
    sommaCumulativa += histogram->GetFrequency( bin, 0 );
    freqCumulativa = sommaCumulativa / totFreq;
    bin++;
  }
  
  threshold_ = (maxValue/255)*bin;
  
  //  std::cout << "Histogram size " << histogramSize << std::endl;
  /* std::cout << "Number of bins = " << histogram->Size() << std::endl  */
  /*           << " Total frequency = " << histogram->GetTotalFrequency() << std::endl  */
  /* 	    << " bin = "         << bin << std::endl  */
  /*   	    << " max = "         << maxValue << std::endl  */
  /* 	    << " value = "         << threshold_ << std::endl  */
  /*           << " Dimension sizes = " << histogram->GetSize() << std::endl; */
}
#endif //CONTACT_CONSTRUCTOR_H

 
